// IWYU pragma: no_include <bits/getopt_core.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "client.h"
#include "convert.h"
#include "ipc.h"
#include "list.h"
#include "log.h"
#include "server.h"

void usage(FILE *stream) {
	static char mesg[] =
		"\n"
		"Usage: way-displays [OPTIONS...] [COMMAND]\n"
		"  Runs the server when no COMMAND specified.\n"
		"OPTIONS\n"
		"  -D, --debug    print debug information\n"
		"COMMANDS\n"
		"  -h, --h[elp]   show this message\n"
		"  -v, --version  display version information\n"
		"  -g, --g[et]    show the active settings\n"
		"  -s, --s[et]    add or change\n"
		"     ARRANGE_ALIGN         <ROW|COLUMN> <TOP|MIDDLE|BOTTOM|LEFT|RIGHT>\n"
		"     ORDER                 <NAME> ...\n"
		"     AUTO_SCALE            <ON|OFF>\n"
		"     SCALE                 <NAME> <SCALE>\n"
		"     MAX_PREFERRED_REFRESH <NAME>\n"
		"     DISABLED              <NAME>\n"
		"  -d, --d[elete] remove\n"
		"     SCALE                 <NAME>\n"
		"     MAX_PREFERRED_REFRESH <NAME>\n"
		"     DISABLED              <NAME>\n"
		"\n"
		;
	fprintf(stream, "%s", mesg);
}

struct Cfg *parse_element(enum IpcRequestCommand command, enum CfgElement element, int argc, char **argv) {
	struct UserScale *user_scale = NULL;

	struct Cfg *cfg = calloc(1, sizeof(struct Cfg));

	bool parsed = false;
	switch (element) {
		case ARRANGE_ALIGN:
			parsed = (cfg->arrange = arrange_val_start(argv[optind]));
			parsed = (cfg->align = align_val_start(argv[optind + 1]));
			break;
		case AUTO_SCALE:
			parsed = (cfg->auto_scale = auto_scale_val(argv[optind]));
			break;
		case SCALE:
			switch (command) {
				case CFG_SET:
					// parse input value
					user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
					user_scale->name_desc = strdup(argv[optind]);
					parsed = ((user_scale->scale = strtof(argv[optind + 1], NULL)) > 0);
					slist_append(&cfg->user_scales, user_scale);
					break;
				case CFG_DEL:
					// dummy values
					for (int i = optind; i < argc; i++) {
						user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
						user_scale->name_desc = strdup(argv[i]);
						user_scale->scale = 1;
						slist_append(&cfg->user_scales, user_scale);
						parsed = true;
					}
					break;
				default:
					break;
			}
			break;
		case DISABLED:
			for (int i = optind; i < argc; i++) {
				slist_append(&cfg->disabled_name_desc, strdup(argv[i]));
			}
			parsed = true;
			break;
		case ORDER:
			for (int i = optind; i < argc; i++) {
				slist_append(&cfg->order_name_desc, strdup(argv[i]));
			}
			parsed = true;
			break;
		case MAX_PREFERRED_REFRESH:
			for (int i = optind; i < argc; i++) {
				slist_append(&cfg->max_preferred_refresh_name_desc, strdup(argv[i]));
			}
			parsed = true;
			break;
		default:
			break;
	}

	if (!parsed) {
		char buf[256];
		char *bp = buf;
		for (int i = optind; i < argc; i++) {
			bp += snprintf(bp, sizeof(buf) - (bp - buf), " %s", argv[i]);
		}
		log_error("invalid %s%s", cfg_element_name(element), buf);
		exit(EXIT_FAILURE);
	}

	return cfg;
}

struct IpcRequest *parse_get(int argc, char **argv) {
	if (optind != argc) {
		log_error("--get takes no arguments");
		exit(EXIT_FAILURE);
	}

	struct IpcRequest *request = calloc(1, sizeof(struct IpcRequest));
	request->command = CFG_GET;

	return request;
}

struct IpcRequest *parse_set(int argc, char **argv) {
	enum CfgElement element = cfg_element_val(optarg);
	switch (element) {
		case ARRANGE_ALIGN:
		case SCALE:
			if (optind + 2 != argc) {
				log_error("%s requires two arguments", cfg_element_name(element));
				exit(EXIT_FAILURE);
			}
			break;
		case AUTO_SCALE:
		case MAX_PREFERRED_REFRESH:
		case DISABLED:
			if (optind + 1 != argc) {
				log_error("%s requires one argument", cfg_element_name(element));
				exit(EXIT_FAILURE);
			}
			break;
		case ORDER:
			if (optind >= argc) {
				log_error("%s requires at least one argument", cfg_element_name(element));
				exit(EXIT_FAILURE);
			}
			break;
		default:
			log_error("invalid %s: %s", ipc_request_command_friendly(CFG_SET), element ? cfg_element_name(element) : optarg);
			exit(EXIT_FAILURE);
	}

	struct IpcRequest *request = calloc(1, sizeof(struct IpcRequest));
	request->command = CFG_SET;
	request->cfg = parse_element(CFG_SET, element, argc, argv);

	return request;
}

struct IpcRequest *parse_del(int argc, char **argv) {
	enum CfgElement element = cfg_element_val(optarg);
	switch (element) {
		case SCALE:
		case MAX_PREFERRED_REFRESH:
		case DISABLED:
			if (optind + 1 != argc) {
				log_error("%s requires one argument", cfg_element_name(element));
				exit(EXIT_FAILURE);
			}
			break;
		default:
			log_error("invalid %s: %s", ipc_request_command_friendly(CFG_DEL), element ? cfg_element_name(element) : optarg);
			exit(EXIT_FAILURE);
	}

	struct IpcRequest *request = calloc(1, sizeof(struct IpcRequest));
	request->command = CFG_DEL;
	request->cfg = parse_element(CFG_DEL, element, argc, argv);

	return request;
}

struct IpcRequest *parse_args(int argc, char **argv) {
	static struct option long_options[] = {
		{ "debug",   no_argument,       0, 'D' },
		{ "delete",  required_argument, 0, 'd' },
		{ "help",    no_argument,       0, 'h' },
		{ "get",     no_argument,       0, 'g' },
		{ "set",     required_argument, 0, 's' },
		{ "version", no_argument,       0, 'v' },
		{ 0,         0,                 0,  0  }
	};
	static char *short_options = "Dd:hgs:v";

	int c;
	while (1) {
		int long_index = 0;
		c = getopt_long(argc, argv, short_options, long_options, &long_index);
		if (c == -1)
			break;
		switch (c) {
			case 'D':
				log_set_threshold(DEBUG);
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case 'v':
				log_info("way-displays version %s", VERSION);
				exit(EXIT_SUCCESS);
			case 'g':
				return parse_get(argc, argv);
			case 's':
				return parse_set(argc, argv);
			case 'd':
				return parse_del(argc, argv);
			case '?':
			default:
				usage(stderr);
				exit(EXIT_FAILURE);
		}
	}

	return NULL;
}

int
main(int argc, char **argv) {
	setlinebuf(stdout);

	struct IpcRequest *ipc_request = parse_args(argc, argv);

	if (ipc_request) {
		return client(ipc_request);
	} else {
		return server();
	}
}

