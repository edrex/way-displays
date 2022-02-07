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
#include "list.h"
#include "log.h"
#include "process.h"
#include "types.h"

int send_messages(struct Cfg *cfg_set, struct Cfg *cfg_del) {
	int fd = -1;
	char *yaml_message = NULL;
	char *yaml_deltas = NULL;
	ssize_t n;

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

void usage() {
	static char *mesg = ""
		"Usage: way-displays [OPTIONS] { COMMAND | help }\n"
		"  OPTIONS\n"
		"    -h, --help     show this message\n"
		"    -v, --version  show program version\n"
		"\n"
		"  COMMANDS\n"
		"    --p[rint]  print the active settings\n"
		"\n"
		"    --s[et]    add or change a setting\n"
		"        ARRANGE    <ROW|COLUMN>\n"
		"        SCALE      <NAME_DESC> <SCALE>\n"
		"\n"
		"    --d[elete] remove a setting\n"
		;
	printf("%s", mesg);
}

bool parse_set(struct Cfg *cfg, int argc, char **argv) {
	int n = 1;
	char *val1 = argv[optind];
	char *val2 = argv[optind + 1];

	if ((strcmp(optarg, "SCALE") == 0)) {
		n = 2;
	}

	if (optind + n - 1 >= argc) {
		fprintf(stderr, "%s requires %d arguments\n\n", optarg, n);
		usage();
		return false;
	}

	if ((strcmp(optarg, "ARRANGE") == 0)) {
		if ((strcmp(val1, "COLUMN") == 0)) {
			set_arrange(cfg, COL);
			cfg->dirty = true;
		} else if ((strcmp(val1, "ROW") == 0)) {
			set_arrange(cfg, ROW);
			cfg->dirty = true;
		} else {
			fprintf(stderr, "invalid ARRANGE: %s\n", val1);
			usage();
			return false;
		}
	} else if ((strcmp(optarg, "SCALE") == 0)) {
		struct UserScale *user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
		user_scale->name_desc = strdup(val1);
		user_scale->scale = atof(val2);
		slist_append(&cfg->user_scales, user_scale);
		cfg->dirty = true;
	} else {
		fprintf(stderr, "invalid set: %s\n\n", optarg);
		usage();
		return false;
	}

	optind += n;

	return true;
}

int client(int argc, char **argv) {
	static struct option long_options[] = {
		{ "help",    no_argument,       0, 'h' },
		{ "print",   no_argument,       0, 'p' },
		{ "set",     required_argument, 0, 's' },
		{ "version", no_argument,       0, 'v' },
		{ 0,         0,                 0,  0  }
	};
	static char *short_options = "hps:v";

	struct Cfg *cfg_set = calloc(1, sizeof(struct Cfg));
	struct Cfg *cfg_del = calloc(1, sizeof(struct Cfg));

	int c;
	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case '?':
				printf("\n");
				usage();
				return EXIT_FAILURE;
			case 'h':
				usage();
				return EXIT_SUCCESS;
			case 'p':
				printf("printing\n");
				return EXIT_SUCCESS;
			case 'v':
				printf("way-displays version %s\n", VERSION);
				return EXIT_SUCCESS;
			case 's':
				if (!parse_set(cfg_set, argc, argv)) {
					return EXIT_FAILURE;
				}
				break;
			default:
				printf("mystery\n");
				break;
		}
	}

	if (cfg_set->dirty || cfg_del->dirty) {
		return send_messages(cfg_set, cfg_del);
	} else {
		fprintf(stderr, "No COMMAND\n\n");
		usage();
		return EXIT_FAILURE;
	}
}

