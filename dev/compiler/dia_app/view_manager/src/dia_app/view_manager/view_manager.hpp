#pragma once
#include <fstream>
#include <json/json.hpp>

#include "common.hpp"
#include "component.hpp"
#include "text_message.hpp"

namespace dia {
	class ViewManager {
	private:
		c_ptr root;

	public:
		ViewManager(const std::string& file_path);

		json query_along_path(ActionPath& path);

		json expand_along_path(ActionPath& path);

		// For unit tests
		json to_json() const;
	};

	c_ptr parse_diagnostics_json(const json& diagnostics) {
		const auto& meta_type = diagnostics[METADATA][TYPE];
		if (meta_type == INTERMEDIATE) {
			const auto&        children = diagnostics[CHILDREN];
			std::vector<c_ptr> children_components;
			std::transform(
				children.begin(),
				children.end(),
				children_components.begin(),
				[](const std::pair<std::string, json>& item) {
					return parse_diagnostics_json(item.second);
				}
			);
			return std::make_shared<Component>(children_components);
		} else if (meta_type == TEXT) {
			return std::make_shared<TextMessage>(diagnostics[CONTENT]);
		} else {
			throw "Unidentified object type: " + to_string(meta_type);
		}
	}

	ViewManager::ViewManager(const std::string& file_path) {
		std::ifstream file(file_path);
		json          diagnostics;
		file >> diagnostics;
		root = parse_diagnostics_json(diagnostics);
	}

	json ViewManager::query_along_path(ActionPath& path) { return this->root->to_json(); }

	json ViewManager::to_json() const { return this->root->to_json(); }

};  // namespace dia
