#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "cfg.h"
#include "client.h"
#include "convert.h"
#include "displ.h"
#include "fds.h"
#include "info.h"
#include "ipc.h"
#include "sockets.h"
#include "layout.h"
#include "lid.h"
#include "log.h"
#include "process.h"
#include "types.h"
#include "wl_wrappers.h"

struct IpcResponse *handle_ipc(int fd_sock, struct Displ *displ) {

	struct IpcResponse *response = (struct IpcResponse*)calloc(1, sizeof(struct IpcResponse));
	response->rc = EXIT_SUCCESS;
	response->done = false;

	struct IpcRequest *request = ipc_request_receive(fd_sock);
	if (request->bad) {
		response->rc = EXIT_FAILURE;
		response->done = true;
		goto end;
	}

	if (request->cfg) {
		log_info("\nServer received %s request:", ipc_request_command_friendly(request->command));
		print_cfg(request->cfg);
	}

	log_capture_start();

	struct Cfg *cfg_merged = NULL;
	switch (request->command) {
		case CFG_SET:
		case CFG_DEL:
			cfg_merged = cfg_merge_request(displ->cfg, request);
			break;
		case CFG_GET:
		default:
			// return the active
			break;
	}

	if (cfg_merged) {
		free_cfg(displ->cfg);
		displ->cfg = cfg_merged;
		displ->cfg->dirty = true;
		log_info("\nApplying new configuration:");
	} else {
		log_info("\nActive configuration:");
	}
	print_cfg(displ->cfg);

end:
	response->fd = request->fd;

	free_ipc_request(request);

	ipc_response_send(response);

	if (response->done) {
		free_ipc_response(response);
		return NULL;
	} else {
		return response;
	}
}

void finish_ipc(struct IpcResponse *response) {
	if (!response) {
		return;
	}

	response->done = true;

	ipc_response_send(response);

	free_ipc_response(response);
}

// see Wayland Protocol docs Appendix B wl_display_prepare_read_queue
int loop(struct Displ *displ) {
	bool user_changes = false;
	bool initial_run_complete = false;
	bool lid_discovery_complete = false;
	struct IpcResponse *ipc_response = NULL;

	init_fds(displ->cfg);
	for (;;) {
		user_changes = false;
		create_pfds(displ);


		// prepare for reading wayland events
		while (_wl_display_prepare_read(displ->display, FL) != 0) {
			_wl_display_dispatch_pending(displ->display, FL);
		}
		_wl_display_flush(displ->display, FL);


		if (!initial_run_complete || lid_discovery_complete) {
			if (poll(pfds, npfds, -1) < 0) {
				log_error_errno("\npoll failed, exiting");
				exit(EXIT_FAILURE);
			}
		} else {
			// takes ~1 sec hence we defer
			displ->lid = create_lid();
			update_lid(displ);
			lid_discovery_complete = true;
		}


		// subscribed signals are mostly a clean exit
		if (pfd_signal && pfd_signal->revents & pfd_signal->events) {
			struct signalfd_siginfo fdsi;
			if (read(fd_signal, &fdsi, sizeof(fdsi)) == sizeof(fdsi)) {
				if (fdsi.ssi_signo != SIGPIPE) {
					return fdsi.ssi_signo;
				}
			}
		}


		// cfg directory change
		if (pfd_cfg_dir && pfd_cfg_dir->revents & pfd_cfg_dir->events) {
			if (cfg_file_written(displ->cfg->file_name)) {
				user_changes = true;
				displ->cfg = cfg_file_reload(displ->cfg);
			}
		}


		// ipc client message
		if (pfd_ipc && pfd_ipc->revents & pfd_ipc->events) {
			ipc_response = handle_ipc(fd_ipc, displ);
			user_changes = (ipc_response != NULL);
		}


		// safe to always read and dispatch wayland events
		_wl_display_read_events(displ->display, FL);
		_wl_display_dispatch_pending(displ->display, FL);


		if (!displ->output_manager) {
			log_info("\nDisplay's output manager has departed, exiting");
			exit(EXIT_SUCCESS);
		}


		// dispatch libinput events only when we have received a change
		if (pfd_lid && pfd_lid->revents & pfd_lid->events) {
			user_changes = user_changes || update_lid(displ);
		}
		// always do this, to cover the initial case
		update_heads_lid_closed(displ);


		// inform of head arrivals and departures and clean them
		user_changes = user_changes || consume_arrived_departed(displ->output_manager);


		// if we have no changes in progress we can maybe react to inital or modified state
		if (is_dirty(displ) && !is_pending_output_manager(displ->output_manager)) {

			// prepare possible changes
			reset_dirty(displ);
			desire_arrange(displ);
			pend_desired(displ);

			if (is_pending_output_manager(displ->output_manager)) {

				// inform and apply
				print_heads(DELTA, displ->output_manager->heads);
				apply_desired(displ);

			} else if (user_changes) {
				log_info("\nNo changes needed");
			}
		}


		// no changes are outstanding
		if (!is_pending_output_manager(displ->output_manager)) {
			if (ipc_response) {
				finish_ipc(ipc_response);
				ipc_response = NULL;
			}
			initial_run_complete = true;
		}


		destroy_pfds();
	}
}

int
server() {

	struct Displ *displ = calloc(1, sizeof(struct Displ));

	log_info("way-displays version %s", VERSION);

	// only one instance
	pid_file_create();

	// always returns a cfg, possibly default
	displ->cfg = cfg_file_load();

	// discover the output manager via a roundtrip
	connect_display(displ);

	// only stops when signalled or display goes away
	int sig = loop(displ);

	// release what remote resources we can
	destroy_display(displ);

	return sig;
}


int
main(int argc, char **argv) {
	setlinebuf(stdout);

	if (argc > 1) {
		return client(argc, argv);
	} else {
		return server();
	}
}

