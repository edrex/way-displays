// IWYU pragma: no_include  <yaml-cpp/emitter.h>
// IWYU pragma: no_include  <yaml-cpp/emittermanip.h>
// IWYU pragma: no_include  <yaml-cpp/node/detail/iterator.h>
// IWYU pragma: no_include  <yaml-cpp/node/impl.h>
// IWYU pragma: no_include  <yaml-cpp/node/iterator.h>
// IWYU pragma: no_include  <yaml-cpp/node/node.h>
// IWYU pragma: no_include  <yaml-cpp/node/parse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml-cpp/yaml.h> // IWYU pragma: keep
#include <exception>
#include <string>

extern "C" {
#include "ipc.h"

#include "cfg.h"
#include "convert.h"
#include "info.h"
#include "list.h"
#include "log.h"
}

char *yaml_with_newline(YAML::Emitter &e) {
	char *yaml = (char*)calloc(e.size() + 2, sizeof(char));
	snprintf(yaml, e.size() + 2, "%s\n", e.c_str());
	return yaml;
}

// TODO maybe back to cfg.cpp
void cfg_emit(YAML::Emitter &e, struct Cfg *cfg) {
	if (!cfg) {
		return;
	}

	e << YAML::BeginMap;

	if (cfg->arrange) {
		e << YAML::Key << "ARRANGE";
		e << YAML::Value << arrange_name(cfg->arrange);
	}

	if (cfg->align) {
		e << YAML::Key << "ALIGN";
		e << YAML::Value << align_name(cfg->align);
	}

	if (cfg->order_name_desc) {
		e << YAML::Key << "ORDER";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->order_name_desc; i; i = i->nex) {
			e << (char*)i->val;
		}
		e << YAML::EndSeq;
	}

	if (cfg->auto_scale) {
		e << YAML::Key << "AUTO_SCALE";
		e << YAML::Value << (cfg->auto_scale == ON);
	}

	if (cfg->user_scales) {
		e << YAML::Key << "SCALE";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->user_scales; i; i = i->nex) {
			struct UserScale *user_scale = (struct UserScale*)i->val;
			e << YAML::BeginMap;
			e << YAML::Key << "NAME_DESC";
			e << YAML::Value << user_scale->name_desc;
			e << YAML::Key << "SCALE";
			e << YAML::Value << user_scale->scale;
			e << YAML::EndMap;
		}
		e << YAML::EndSeq;
	}

	if (cfg->laptop_display_prefix) {
		e << YAML::Key << "LAPTOP_DISPLAY_PREFIX";
		e << YAML::Value << cfg->laptop_display_prefix;
	}

	if (cfg->max_preferred_refresh_name_desc) {
		e << YAML::Key << "MAX_PREFERRED_REFRESH";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->max_preferred_refresh_name_desc; i; i = i->nex) {
			e << (char*)i->val;
		}
		e << YAML::EndSeq;
	}

	if (cfg->disabled_name_desc) {
		e << YAML::Key << "DISABLED";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->disabled_name_desc; i; i = i->nex) {
			e << (char*)i->val;
		}
		e << YAML::EndSeq;
	}

	e << YAML::EndMap;
}

char *ipc_marshal_request(struct IpcRequest *request) {
	if (!request) {
		return NULL;
	}

	try {
		YAML::Emitter e;

		e << YAML::TrueFalseBool;
		e << YAML::UpperCase;

		e << YAML::BeginMap;

		e << YAML::Key << ipc_request_command_name(request->command);

		if (request->cfg) {
			cfg_emit(e, request->cfg);
		} else {
			e << YAML::Value << "";
		}

		e << YAML::EndMap;

		if (!e.good()) {
			log_error("marshalling ipc request: %s", e.GetLastError().c_str());
			return NULL;
		}

		return yaml_with_newline(e);

	} catch (const std::exception &e) {
		log_error("marshalling ipc request: %s\n%s", e.what());
		return NULL;
	}
}

struct IpcRequest *ipc_unmarshal_request(char *yaml) {
	if (!yaml) {
		return NULL;
	}

	struct IpcRequest *request = (struct IpcRequest*)calloc(1, sizeof(struct IpcRequest));

	try {
		YAML::Node node = YAML::Load(yaml);
		if (!node.IsMap()) {
			throw std::runtime_error("no commands");
		}

		if (node.size() != 1) {
			throw std::runtime_error("multiple commands");
		}

		request->command = ipc_request_command_val(node.begin()->first.as<std::string>().c_str());
		if (!request->command) {
			throw std::runtime_error("invalid command");
		}

		switch (request->command) {
			case CFG_ADD:
			case CFG_SET:
			case CFG_DEL:
				request->cfg = (struct Cfg*)calloc(1, sizeof(struct Cfg));
				cfg_parse_node(request->cfg, node.begin()->second);
				break;
			case CFG_GET:
			default:
				break;
		}

		return request;

	} catch (const std::exception &e) {
		log_error("unmarshalling ipc request: %s\n----------------------------------------\n%s\n----------------------------------------", e.what(), yaml);
		free_ipc_request(request);
		return NULL;
	}
}

char *ipc_marshal_response(struct IpcResponse *response) {
	char *yaml = NULL;

	try {
		YAML::Emitter e;

		e << YAML::TrueFalseBool;
		e << YAML::UpperCase;

		e << YAML::BeginMap;

		e << YAML::Key << ipc_response_field_name(DONE);
		e << YAML::Value << response->done;

		e << YAML::Key << ipc_response_field_name(RC);
		e << YAML::Value << response->rc;

		if (log_cap.num_lines > 0) {

			e << YAML::Key << ipc_response_field_name(MESSAGES);

			e << YAML::BeginSeq;

			for (size_t i = 0; i < log_cap.num_lines; i++) {
				struct LogCapLine *cap_line = log_cap.lines[i];
				if (cap_line && cap_line->line) {
					e << cap_line->line;
				}
			}

			e << YAML::EndSeq;
		}

		e << YAML::EndMap;

		if (!e.good()) {
			log_error("marshalling ipc response: %s", e.GetLastError().c_str());
			return NULL;
		}

		yaml = yaml_with_newline(e);

	} catch (const std::exception &e) {
		log_error("marshalling ipc response: %s\n%s", e.what());
	}

	log_capture_reset();

	return yaml;
}

struct IpcResponse *ipc_unmarshal_response(char *yaml) {
	struct IpcResponse *response = (struct IpcResponse*)calloc(1, sizeof(struct IpcResponse));
	response->rc = EXIT_FAILURE;
	response->done = true;

	if (!yaml) {
		return response;
	}

	try {
		const YAML::Node node = YAML::Load(yaml);

		for(YAML::const_iterator it = node.begin(); it!=node.end(); ++it) {

			if (it->first.as<std::string>() == ipc_response_field_name(DONE)) {
				response->done = it->second.as<bool>();
			}

			if (it->first.as<std::string>() == ipc_response_field_name(RC)) {
				response->rc = it->second.as<int>();
			}

			if (it->first.as<std::string>() == ipc_response_field_name(MESSAGES)) {
				for (const auto &message : it->second) {
					printf("%s\n", message.as<std::string>().c_str());
				}
			}
		}

	} catch (const std::exception &e) {
		log_error("unmarshalling ipc response: %s\n----------------------------------------\n%s\n----------------------------------------", e.what(), yaml);
		response->rc = EXIT_FAILURE;
	}

	return response;
}

void free_ipc_request(struct IpcRequest *request) {
	if (!request) {
		return;
	}

	free_cfg(request->cfg);

	free(request);
}

void free_ipc_response(struct IpcResponse *response) {
	if (!response) {
		return;
	}

	free(response);
}

