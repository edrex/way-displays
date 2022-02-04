#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc.h"

#include "cfg.h"
#include "list.h"
#include "log.h"
#include "process.h"
#include "types.h"

#define READ_TIMEOUT_USEC 800000

bool set_socket_timeout(int fd) {

	static struct timeval timeout = {.tv_sec = 0, .tv_usec = READ_TIMEOUT_USEC};
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
		log_error_errno("socket set timeout failed");
		return false;
	}

	return true;
}

int accept_socket(int fd_sock) {

	int fd = accept(fd_sock, NULL, NULL);
	if (fd == -1) {
		log_error_errno("socket accept failed");
		return -1;
	}

	if (!set_socket_timeout(fd)) {
		return -1;
	}

	return fd;
}

char *read_from_socket(int fd) {

	// peek, as the client may experience delay between connecting and sending
	if (recv(fd, NULL, 0, MSG_PEEK) == -1) {
		if (errno == EAGAIN) {
			log_error("socket read timeout");
		} else {
			log_error_errno("socket recv failed");
		}
		return NULL;
	}

	// total message size right now; further data will be disregarded
	int n = 0;
	if (ioctl(fd, FIONREAD, &n) == -1) {
		log_error_errno("server FIONREAD failed");
		return NULL;
	}
	if (n == 0) {
		log_error("socket no data");
		return NULL;
	}

	// read it
	char *buf = calloc(n + 1, sizeof(char));
	if (recv(fd, buf, n, 0) == -1) {
		log_error_errno("socket recv failed");
		return NULL;
	}

	return buf;
}

ssize_t write_to_socket(int fd, char *data, size_t len) {

	ssize_t n;
	if ((n = write(fd, data, len)) == -1) {
		log_error_errno("socket write failed");
		return -1;
	}

	return n;
}

void socket_path(struct sockaddr_un *addr) {
	size_t sun_path_size = sizeof(addr->sun_path);

	const char *xdg_vtnr = getenv("XDG_VTNR");

	char name[sun_path_size];
	if (xdg_vtnr) {
		snprintf(name, sizeof(name), "/way-displays.%s.sock", xdg_vtnr);
	} else {
		snprintf(name, sizeof(name), "/way-displays.sock");
	}

	const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!xdg_runtime_dir || strlen(name) + strlen(xdg_runtime_dir) > sun_path_size) {
		snprintf(addr->sun_path, sun_path_size, "/tmp%s", name);
	} else {
		snprintf(addr->sun_path, sun_path_size, "%s%s", xdg_runtime_dir, name);
	}
}

int create_fd_ipc_server() {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		log_error_errno("server socket failed, clients unavailable");
		return -1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	socket_path(&addr);
	unlink(addr.sun_path);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		log_error_errno("server socket bind failed, clients unavailable");
		close(fd);
		return -1;
	}

	if (listen(fd, 3) < 0) {
		log_error_errno("server socket listen failed, clients unavailable");
		close(fd);
		return -1;
	}

	return fd;
}

int create_fd_ipc_client() {

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		log_error_errno("socket create failed");
		return -1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	socket_path(&addr);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		log_error_errno("socket connect failed");
		close(fd);
		return -1;
	}

	if (!set_socket_timeout(fd)) {
		close(fd);
		return -1;
	}

	return fd;
}

bool process_ipc_message(int fd_sock, struct Displ *displ) {
	if (fd_sock == -1 || !displ || !displ->cfg) {
		return false;
	}

	bool rc = true;
	struct Cfg *cfg_new = NULL;
	int fd = -1;
	char *yaml_message = NULL;
	char *yaml_active = NULL;
	ssize_t n;

	if ((fd = accept_socket(fd_sock)) == -1) {
		rc = false;
		goto end;
	}

	if (!(yaml_message = read_from_socket(fd))) {
		rc = false;
		goto end;
	}
	log_debug("server read\n----\n%s\n----", yaml_message);

	if (!(cfg_new = merge_cfg(displ->cfg, yaml_message))) {
		rc = false;
		goto end;
	}

	free_cfg(displ->cfg);
	displ->cfg = cfg_new;
	displ->cfg->dirty = true;

	yaml_active = cfg_yaml_active(displ->cfg);
	if (!yaml_active) {
		rc = false;
		goto end;
	}
	log_debug("server sending\n====\n%s\n====", yaml_active);
	if ((n = write_to_socket(fd, yaml_active, strlen(yaml_active))) == -1) {
		rc = false;
		goto end;
	}
	log_debug("server wrote %ld", n);

end:
	if (fd != -1) {
		close(fd);
	}

	free(yaml_active);
	free(yaml_message);

	return rc;
}

int client_stuff(int argc, const char **argv) {
	int fd = -1;
	char *yaml_message = NULL;
	char *yaml_deltas = NULL;
	ssize_t n;

	struct Cfg *cfg_set = calloc(1, sizeof(struct Cfg));
	set_arrange(cfg_set, COL);
	set_align(cfg_set, BOTTOM);
	set_auto_scale(cfg_set, true);
	cfg_set->laptop_display_prefix = strdup("aoeu");
	slist_append(&cfg_set->order_name_desc, strdup("THREE"));
	slist_append(&cfg_set->disabled_name_desc, strdup("900"));
	slist_append(&cfg_set->max_preferred_refresh_name_desc, strdup("AAA"));
	struct UserScale *scale = calloc(1, sizeof(struct UserScale));
	scale->name_desc = strdup("SCALY");
	scale->scale = 22;
	slist_append(&cfg_set->user_scales, scale);


	struct Cfg *cfg_del = calloc(1, sizeof(struct Cfg));

	char *ord = strdup("DP-2");
	slist_append(&cfg_del->order_name_desc, ord);

	char *mpr = strdup("HDMI-1");
	slist_append(&cfg_del->max_preferred_refresh_name_desc, mpr);

	struct UserScale *scl = calloc(1, sizeof(struct UserScale));
	scl->name_desc = strdup("eDP-1");
	scl->scale = 1;
	slist_append(&cfg_del->user_scales, scl);

	char *dis = strdup("eDP-2");
	slist_append(&cfg_del->disabled_name_desc, dis);

	yaml_deltas = cfg_yaml_deltas(cfg_set, cfg_del);
	if (!yaml_deltas) {
		return EXIT_FAILURE;
	}
	log_debug("client sending\n----\n%s\n----", yaml_deltas);

	if (running_pid() == 0) {
		log_error("way-displays server not running, exiting");
		return EXIT_FAILURE;
	}

	if ((fd = create_fd_ipc_client()) == -1) {
		return EXIT_FAILURE;
	}

	if ((n = write_to_socket(fd, yaml_deltas, strlen(yaml_deltas))) == -1) {
		return EXIT_FAILURE;
	}
	log_debug("client wrote %ld\n'", n);

	if (!(yaml_message = read_from_socket(fd))) {
		close(fd);
		return EXIT_FAILURE;
	}
	log_debug("client read\n====\n%s\n====", yaml_message);



	close(fd);

	free_cfg(cfg_set);
	free_cfg(cfg_del);

	free(yaml_message);
	free(yaml_deltas);

	return EXIT_SUCCESS;
}
