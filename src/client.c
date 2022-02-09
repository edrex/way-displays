#include <errno.h>
#include <getopt.h>
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
#include "convert.h"
#include "info.h"
#include "list.h"
#include "log.h"
#include "process.h"
#include "types.h"

bool execute(struct Cfg *cfg_set, struct Cfg *cfg_del) {
	bool success = true;
	int fd = -1;
	char *yaml_request = NULL;
	char *yaml_response = NULL;
	struct Cfg *cfg_active = NULL;
	ssize_t n;

	yaml_request = cfg_deltas_yaml(cfg_set, cfg_del);
	if (!yaml_request) {
		success = false;
		goto end;
	}

	if ((fd = create_fd_ipc_client()) == -1) {
		success = false;
		goto end;
	}

	if ((n = socket_write(fd, yaml_request, strlen(yaml_request))) == -1) {
		success = false;
		goto end;
	}
	log_debug("\nSent request:\n%s", yaml_request);

	if (!(yaml_response = socket_read(fd))) {
		close(fd);
		success = false;
		goto end;
	}
	log_debug("\nReceived response:\n%s", yaml_response);

	// TODO maybe server response
	cfg_print_messages(yaml_response);

	cfg_active = calloc(1, sizeof(struct Cfg));
	if (cfg_parse_active_yaml(cfg_active, yaml_response)) {
		log_info("\nActive Configuration:");
		print_cfg(cfg_active);
	} else {
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
	free_cfg(cfg_active);

	return success;
}

void usage(FILE *stream) {
	static char mesg[] =
		"\n"
		"Usage: way-displays [OPTIONS] <COMMAND> ...\n"
		"\n"
		"  OPTIONS\n"
		"    -D, --debug    show this message\n"
		"    -h, --help     show this message\n"
		"    -v, --version  show program version\n"
		"\n"
		"  command\n"
		"    -p, --p[rint]   print the active settings\n"
		"\n"
		"    -s, --s[et]     change or add to list\n"
		"        ARRANGE               <ROW | COLUMN>\n"
		"        ALIGN                 <TOP | MIDDLE | BOTTOM | LEFT | RIGHT>\n"
		"        ORDER                 <NAME_DESC>\n"
		"        AUTO_SCALE            <ON | OFF>\n"
		"        SCALE                 <NAME_DESC> <SCALE>\n"
		"        MAX_PREFERRED_REFRESH <NAME_DESC>\n"
		"        DISABLED              <NAME_DESC>\n"
		"\n"
		"    -d, --d[elete]  remove from list\n"
		"        ORDER                 <NAME_DESC>\n"
		"        SCALE                 <NAME_DESC>\n"
		"        MAX_PREFERRED_REFRESH <NAME_DESC>\n"
		"        DISABLED              <NAME_DESC>\n"
		"\n"
		"Example: turn on auto scale, disable HDMI-1 and remove eDP-1's custom scale:\n"
		"  way-displays --set AUTO_SCALE ON --set DISABLED HDMI-1 --del SCALE eDP-1\n"
		"\n"
		"TODO: describe running as a server\n"
		"\n"
		;
	fprintf(stream, "%s", mesg);
}

void parse_set(struct Cfg **cfg, int argc, char **argv) {
	struct UserScale *user_scale = NULL;
	char *val1 = NULL;
	char *val2 = NULL;

	enum CfgElement cfg_element = cfg_element_val(optarg);
	switch (cfg_element) {
		case ARRANGE:
		case ALIGN:
		case ORDER:
		case AUTO_SCALE:
		case MAX_PREFERRED_REFRESH:
		case DISABLED:
			if (optind >= argc) {
				log_error("%s requires an argument", optarg);
				exit(EXIT_FAILURE);
			}
			val1 = argv[optind];
			break;
		case SCALE:
			if (optind + 1 >= argc) {
				log_error("%s requires two arguments", optarg);
				exit(EXIT_FAILURE);
			}
			val1 = argv[optind];
			val2 = argv[optind + 1];
			break;
		default:
			log_error("invalid --set %s", optarg);
			exit(EXIT_FAILURE);
	}

	if (!*cfg) {
		*cfg = calloc(1, sizeof(struct Cfg));
	}
	bool parsed = false;
	switch (cfg_element) {
		case ARRANGE:
			parsed = ((*cfg)->arrange = arrange_val(val1));
			break;
		case ALIGN:
			parsed = ((*cfg)->align = align_val(val1));
			break;
		case ORDER:
			slist_append(&(*cfg)->order_name_desc, strdup(val1));
			parsed = true;
			break;
		case AUTO_SCALE:
			parsed = ((*cfg)->auto_scale = auto_scale_val(val1));
			break;
		case SCALE:
			user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
			user_scale->name_desc = strdup(val1);
			parsed = ((user_scale->scale = strtof(val2, NULL)) > 0);
			slist_append(&(*cfg)->user_scales, user_scale);
			break;
		case MAX_PREFERRED_REFRESH:
			slist_append(&(*cfg)->max_preferred_refresh_name_desc, strdup(val1));
			parsed = true;
			break;
		case DISABLED:
			slist_append(&(*cfg)->disabled_name_desc, strdup(val1));
			parsed = true;
			break;
		default:
			break;
	}

	if (!parsed) {
		if (val2) {
			log_error("invalid --set %s %s %s", optarg, val1, val2);
		} else {
			log_error("invalid --set %s %s", optarg, val1);
		}
		exit(EXIT_FAILURE);
	}
}

void parse_del(struct Cfg **cfg, int argc, char **argv) {
	struct UserScale *user_scale = NULL;
	char *val = NULL;

	enum CfgElement cfg_element = cfg_element_val(optarg);
	switch (cfg_element) {
		case ORDER:
		case SCALE:
		case MAX_PREFERRED_REFRESH:
		case DISABLED:
			if (optind >= argc) {
				log_error("%s requires an argument", optarg);
				exit(EXIT_FAILURE);
			}
			val = argv[optind];
			break;
		default:
			log_error("invalid --del %s", optarg);
			exit(EXIT_FAILURE);
	}

	if (!*cfg) {
		*cfg = calloc(1, sizeof(struct Cfg));
	}
	bool parsed = false;
	switch (cfg_element) {
		case ORDER:
			slist_append(&(*cfg)->order_name_desc, strdup(val));
			parsed = true;
			break;
		case SCALE:
			user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
			user_scale->name_desc = strdup(val);
			user_scale->scale = 1;
			slist_append(&(*cfg)->user_scales, user_scale);
			parsed = true;
			break;
		case MAX_PREFERRED_REFRESH:
			slist_append(&(*cfg)->max_preferred_refresh_name_desc, strdup(val));
			parsed = true;
			break;
		case DISABLED:
			slist_append(&(*cfg)->disabled_name_desc, strdup(val));
			parsed = true;
			break;
		default:
			break;
	}

	if (!parsed) {
		log_error("invalid --del %s %s", optarg, val);
		exit(EXIT_FAILURE);
	}
}

int parse_args(int argc, char **argv, struct Cfg **cfg_set, struct Cfg **cfg_del) {
	static struct option long_options[] = {
		{ "debug",   no_argument,       0, 'D' },
		{ "delete",  required_argument, 0, 'd' },
		{ "help",    no_argument,       0, 'h' },
		{ "print",   no_argument,       0, 'p' },
		{ "set",     required_argument, 0, 's' },
		{ "version", no_argument,       0, 'v' },
		{ 0,         0,                 0,  0  }
	};
	static char *short_options = "Dd:hps:v";

	int commands = 0;
	int c;
	while (1) {
		int long_index = 0;
		c = getopt_long(argc, argv, short_options, long_options, &long_index);
		if (c == -1)
			break;
		switch (c) {
			case '?':
				usage(stderr);
				exit(EXIT_FAILURE);
			case 'D':
				log_level = LOG_LEVEL_DEBUG;
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case 'p':
				commands++;
				break;
			case 'v':
				printf("way-displays version %s\n", VERSION);
				exit(EXIT_SUCCESS);
			case 'd':
				commands++;
				parse_del(cfg_del, argc, argv);
				break;
			case 's':
				commands++;
				parse_set(cfg_set, argc, argv);
				break;
			default:
				break;
		}
	}

	return commands;
}

int client(int argc, char **argv) {
	log_level = LOG_LEVEL_INFO;
	log_time = false;

	struct Cfg *cfg_set = NULL;
	struct Cfg *cfg_del = NULL;

	if (parse_args(argc, argv, &cfg_set, &cfg_del) == 0) {
		log_error("no COMMAND specified");
		usage(stderr);
		exit(EXIT_FAILURE);
	}

	if (pid_active_server() == 0) {
		log_error("way-displays server not running");
		exit(EXIT_FAILURE);
	}

	print_cfg_deltas(cfg_set, cfg_del);

	if (!execute(cfg_set, cfg_del)) {
		exit(EXIT_FAILURE);
	}

	free_cfg(cfg_set);
	free_cfg(cfg_del);

	return EXIT_SUCCESS;
}

