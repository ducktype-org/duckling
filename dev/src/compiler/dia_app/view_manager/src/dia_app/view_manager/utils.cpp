#include "utils.hpp"

#include "dia_parser.hpp"
#include "template_application.hpp"
#include "template_parser.hpp"

namespace dia_app {
	std::string MESSAGE_TEMPLATE_PATH
		= "../src/compiler/dia_app/view_manager/src/dia_app/view_manager/templates/";

	ExploreEdgeParams::ExploreEdgeParams(const json &edge)  {
		ASSUME_OBJ(edge);
		ASSUME_HAS_STR_ASSIGN(edge, name);
		ASSUME_HAS_UINT_ASSIGN(edge, handle);
		ASSUME_HAS(edge, "params");
		ASSUME_OBJ(edge["params"]);
		for (auto &[key, val] : edge["params"].items()) {
			params.put(key, dia_file::parse(val));
		}
	}

	DataHandle::DataHandle(
		base::HashMap<std::string, json>&            entities,
		std::vector<ParamData>&                 secondary_infos,
		base::HashMap<InfoHandle, InfoParamsHandle>& info_handles
	):
		  entities(entities),
		  secondary_infos(secondary_infos),
		  info_handles(info_handles) {}

	TemplateDataHandle::TemplateDataHandle(
		const TemplateData& template_data, const ParamData& param_data, DataHandle data_handle
	):
		  template_data(template_data),
		  param_data(param_data),
		  entities(data_handle.entities),
		  secondary_infos(data_handle.secondary_infos),
		  info_handles(data_handle.info_handles) {}

	TemplateDataHandle TemplateDataHandle::with_aux_params(const base::HashMap<std::string, dia_file::Ptr> &aux_params) const {
		TemplateDataHandle res(*this);
		res.aux_params = aux_params;
		return res;
	}

	DataHandle TemplateDataHandle::toDataHandle() const {
		return DataHandle(entities, secondary_infos, info_handles);
	}

	InfoParamsHandle::InfoParamsHandle(const json& handle_json) {
		if (handle_json.is_number_unsigned()) {
			idx = handle_json;
			return;
		}
		// Parsing a dummy handle - simply the param data
		// of the info. To be replaced by some LS-generated handle
		// for later access.
		ASSUME_HAS_STR(handle_json, "type");
		ASSUME_VAL(handle_json, "type", "dummy_handle");
		ASSUME_HAS(handle_json, "handle");
		param_data = handle_json["handle"];
	}

	u32 InfoParamsHandle::load(DataHandle dh) {
		if_opt_some(idx, idx_v) {
			return idx_v;
		}
		if_opt_some(metadata, metadata_v) {
			// Add a new info given metadata.
			dh.secondary_infos.push_back(dia_file::ParamData(metadata_v));
			// Update the handle.
			idx = dh.secondary_infos.size() - 1;
			metadata.reset();
			// Return the new info's index.
			return idx.value();
		}
		// Otherwise it is a dummy handle.
		dh.secondary_infos.push_back(param_data.value());

		idx = dh.secondary_infos.size() - 1;
		param_data.reset();
		return idx.value();
	}

	u32 InfoParamsHandle::load(InfoHandle info, DataHandle dh) {
		return dh.info_handles.at(info).load(dh);
	}

	InfoHandle InfoParamsHandle::add(const InfoParamsHandle& params_handle, DataHandle dh) {
		// Check if the handle already exists.
		for (auto& [key, val]: dh.info_handles)
			if (val == params_handle) return key;
		// If not, create a new one and return a stateless handle to it.
		InfoHandle handle       = dh.info_handles.size();
		dh.info_handles.put(handle, params_handle);
		return handle;
	}
}
