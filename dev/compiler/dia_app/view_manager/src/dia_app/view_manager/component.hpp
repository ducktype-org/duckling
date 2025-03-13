#pragma once
#include "action_path.hpp"
#include "common.hpp"
#include <json/json.hpp>

namespace dia {
	class Component;

	// Pointer to component.
	using c_ptr = ptr<Component>;
	using json  = nlohmann::json;

	json create_intermediate_component_json(const std::vector<json>& children) {
		return json{ { METADATA, { TYPE, INTERMEDIATE } }, { CHILDREN, children } };
	}

	class Component {
	protected:
		std::vector<c_ptr> children;

	public:
		Component(std::vector<c_ptr> children): children(children) {}

		json traverse_path(const ActionPath& path) {}

		json to_json() const {
			std::vector<json> children_json;
			std::transform(
				children.begin(),
				children.end(),
				children_json.begin(),
				[](c_ptr child) { return child->to_json(); }
			);
			return create_intermediate_component_json(children_json);
		}
	};
}  // namespace dia
