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


	TemplateDataHandle::TemplateDataHandle(
		ViewConstructor &vc, const TemplateData& template_data, const InfoParams& param_data
	):
		  vc(vc),
		  template_data(template_data),
		  param_data(param_data) {}

	TemplateDataHandle TemplateDataHandle::with_aux_params(const base::HashMap<std::string, dia_file::DisplayPtr> &aux_params) const {
		TemplateDataHandle res(*this);
		res.aux_params = aux_params;
		return res;
	}

}
