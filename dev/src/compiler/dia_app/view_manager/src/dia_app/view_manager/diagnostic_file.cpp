#include "diagnostic_file.hpp"

namespace dia_app::dia_file {
	static auto fromJson(const json& elem) -> Box<Component> {
		ASSUME_HAS(elem, "type");
		std::string type = elem["type"];
		if (type == "text")
			return TextComponent::fromJson(elem);
		else if (type == "code")
			return CodeComponent::fromJson(elem);
		else if (type == "code_with_location")
			return CodeWithLocationComponent::fromJson(elem);
		else if (type == "start_line")
			return StartLineComponent::fromJson(elem);
		else if (type == "concat")
			return ConcatComponent::fromJson(elem);
		else if (type == "pointed")
			return PointedComponent::fromJson(elem);
		else if (type == "variant")
			return VariantComponent::fromJson(elem);
		else if (type == "entity")
			return EntityComponent::fromJson(elem);
		else
			CORE_PANIC("Unknown component type '{}'.", type);
	}
}
