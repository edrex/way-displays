#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <wayland-client-core.h>

#include "fds.h"

#include "log.h"
#include "types.h"

int fd_signal = 0;
int fd_cfg_dir = 0;

int npfds = 0;
struct pollfd *pfds = NULL;

struct pollfd *pfd_signal = NULL;
struct pollfd *pfd_wayland = NULL;
struct pollfd *pfd_lid = NULL;
struct pollfd *pfd_cfg_dir = NULL;

int create_fd_signal() {
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	return signalfd(-1, &mask, 0);
}

int create_fd_cfg_dir(struct Cfg *cfg) {
	if (!cfg || !cfg->dir_path)
		return 0;

	fd_cfg_dir = inotify_init1(IN_NONBLOCK);
	if (inotify_add_watch(fd_cfg_dir, cfg->dir_path, IN_CLOSE_WRITE) == -1) {
		log_error_errno("\nunable to create config file watch for %s, exiting", cfg->dir_path);
		exit(EXIT_FAILURE);
	}

	return fd_cfg_dir;
}

void init_pfds(struct Cfg *cfg) {
	fd_signal = create_fd_signal();
	fd_cfg_dir = create_fd_cfg_dir(cfg);
}

void create_pfds(struct Displ *displ) {
	if (!displ || !displ->display)
		return;

	// wayland and signal are always present, others are optional
	npfds = 2;
	if (displ->lid)
		npfds++;
	if (fd_cfg_dir)
		npfds++;

	pfds = calloc(npfds, sizeof(struct pollfd));

	int i = 0;

	pfd_signal = &pfds[i++];
	pfd_signal->fd = fd_signal;
	pfd_signal->events = POLLIN;

	pfd_wayland = &pfds[i++];
	pfd_wayland->fd = wl_display_get_fd(displ->display);
	pfd_wayland->events = POLLIN;

	if (displ->lid) {
		pfd_lid = &pfds[i++];
		pfd_lid->fd = displ->lid->libinput_fd;
		pfd_lid->events = POLLIN;
	}

	if (fd_cfg_dir) {
		pfd_cfg_dir = &pfds[i++];
		pfd_cfg_dir->fd = fd_cfg_dir;
		pfd_cfg_dir->events = POLLIN;
	}
}

void destroy_pfds() {
	npfds = 0;

	pfd_signal = NULL;
	pfd_wayland = NULL;
	pfd_lid = NULL;
	pfd_cfg_dir = NULL;

	free(pfds);
}

// see man 7 inotify
bool cfg_file_written(char *file_name) {

	char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;
	ssize_t len;

	while ((len = read(fd_cfg_dir, buf, sizeof(buf))) > 0) {
		for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
			event = (const struct inotify_event *) ptr;
			if (event->mask & IN_CLOSE_WRITE && event->len && strcmp(file_name, event->name) == 0) {
				return true;
			}
		}
	}

	return false;
}

