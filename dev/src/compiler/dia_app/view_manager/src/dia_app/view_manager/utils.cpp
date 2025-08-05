#include "utils.hpp"

#include "dia_parser.hpp"
#include "template_application.hpp"
#include "template_parser.hpp"

namespace dia_app {
	std::string MESSAGE_TEMPLATE_PATH
		= "../src/compiler/dia_app/view_manager/src/dia_app/view_manager/templates/";
	
	ShortMetadata::ShortMetadata() {}
	ShortMetadata::ShortMetadata(const json &metadata) {
		ASSUME_HAS_STR(metadata, "type");
		type = from_string(metadata["type"]);
		ASSUME_HAS_STR_ASSIGN(metadata, family);
		ASSUME_HAS_STR_ASSIGN(metadata, name);
	}
	ShortMetadata::ShortMetadata(const YAML::Node &node) {
		// Required fields
		assert(node["type"] && node["type"].IsScalar());
		assert(node["family"] && node["family"].IsScalar());
		assert(node["name"] && node["name"].IsScalar());

		type = from_string(node["type"].as<std::string>());
		family = node["family"].as<std::string>();
		name = node["name"].as<std::string>();
	}

	std::string ShortMetadata::getPath() const {
		return MESSAGE_TEMPLATE_PATH + to_string(type) + '/' + family + '/' + name + ".yaml";
	}

	bool ShortMetadata::operator==(const ShortMetadata &other) const {
		return type == other.type && family == other.family && name == other.name;
	}

	
	Metadata::Metadata() {}
	Metadata::Metadata(const YAML::Node &node) {
		// Required fields
		assert(node["type"] && node["type"].IsScalar());
		assert(node["family"] && node["family"].IsScalar());
		assert(node["name"] && node["name"].IsScalar());
		assert(node["code"] && node["code"].IsScalar());
		assert(node["active_from"] && node["active_from"].IsScalar());
		assert(node["active_until"] && node["active_until"].IsScalar());

		type = from_string(node["type"].as<std::string>());
		family = node["family"].as<std::string>();
		name = node["name"].as<std::string>();
		try {
			code = std::stoi(node["code"].as<std::string>());
		} catch (const std::logic_error &e) {
			ASSUME(false, "message code must be convertible to an integer, instead provided: " << node["code"].as<std::string>());
		}
		active_from = node["active_from"].as<std::string>();
		active_until = node["active_until"].as<std::string>();
	}

	bool Metadata::sameAs(const ShortMetadata &metadata) const {
		return type == metadata.type
			&& family == metadata.family
			&& name == metadata.name;
	}

}
