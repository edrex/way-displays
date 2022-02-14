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

char *process_client_request(struct Displ *displ, char *yaml_request) {
	int rc_client = EXIT_SUCCESS;
	char *yaml_response = NULL;

	log_debug(" \n--------received client request---------\n%s\n----------------------------------------", yaml_request);

	// TODO wrap log capture around this
	struct IpcRequest *request = ipc_unmarshal_request(yaml_request);
	if (!request) {
		rc_client = EXIT_FAILURE;
		goto end;
	}

	log_info("\nServer received %s request:", ipc_command_friendly(request->command));
	if (request->cfg) {
		print_cfg(request->cfg);
	}

	log_capture_start();

	struct Cfg *cfg_merged = NULL;
	switch (request->command) {
		case CFG_ADD:
		case CFG_SET:
		case CFG_DEL:
			cfg_merged = cfg_merge_request(displ->cfg, request);
			if (!cfg_merged) {
				rc_client = EXIT_FAILURE;
				goto end;
			}
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
		log_info("\nNew configuration applied:");
	} else {
		log_info("\nActive configuration:");
	}
	print_cfg(displ->cfg);

end:
	yaml_response = ipc_marshal_response(rc_client);

	log_capture_end();

	free_ipc_request(request);

	return yaml_response;
}

// TODO move to server.c
bool process_ipc_request(int fd_sock, struct Displ *displ) {
	if (fd_sock == -1 || !displ || !displ->cfg) {
		return false;
	}

	bool success = true;
	int fd = -1;
	char *yaml_request = NULL;
	char *yaml_response = NULL;

	if ((fd = socket_accept(fd_sock)) == -1) {
		success = false;
		goto end;
	}

	if (!(yaml_request = socket_read(fd))) {
		success = false;
		goto end;
	}

	log_debug("\n--------received client request---------\n%s\n----------------------------------------", yaml_request);

	yaml_response = process_client_request(displ, yaml_request);

	if (!yaml_response) {
		success = false;
		goto end;
	}

	log_debug("\n--------sending client response----------\n%s\n----------------------------------------", yaml_response);

	if (socket_write(fd, yaml_response, strlen(yaml_response)) == -1) {
		success = false;
		goto end;
	}

end:
	if (fd != -1) {
		close(fd);
	}
	if (yaml_request) {
		free(yaml_request);
	}
	if (yaml_response) {
		free(yaml_response);
	}

	return success;
}

// see Wayland Protocol docs Appendix B wl_display_prepare_read_queue
int loop(struct Displ *displ) {
	bool user_changes = false;
	bool initial_run_complete = false;
	bool lid_discovery_complete = false;

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
			// TODO delay the response until success or failure of changes
			user_changes = process_ipc_request(fd_ipc, displ);
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

