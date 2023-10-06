/**
 * @file templates.hpp
 * @brief not fully implemented TemplateInfo
 */

#pragma once

#include "type_desc.hpp"
#include "type_desc.tcpp"
#include "type_info.hpp"

#include <exec/ctv.hpp>

namespace ts {
	namespace external {
		class TemplateSchematic;
	}

	class TemplateInfo: public TypeInfo {
		SETUP_TYPE(TemplateInfo, TypeInfo)

	public:
		std::vector<TypeDesc<>> getParameterList() const;

		external::TemplateSchematic* getTemplateSchematic() const;

		// @TODO: Change return type to baked template when it's created.
		TypeInfo bake(std::vector<exec::CTV>& arg_list) const;

		CHECKED_CAST(TemplateInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(TemplateInfo)
	};

	class TypeTemplateInfo: public TemplateInfo {
		SETUP_TYPE(TypeTemplateInfo, TemplateInfo)

	public:
		static TypeTemplateInfo create(std::vector<TypeDesc<>>& parameter_list);

		CHECKED_CAST(TypeTemplateInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(TypeTemplateInfo)
	};
}
