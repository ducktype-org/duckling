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
	public:
		SETUP_TYPE_WITH_BASE(TemplateInfo, TypeInfo)
		std::vector<TypeDesc<>> getParameterList() const;

		external::TemplateSchematic* getTemplateSchematic() const;

		// @TODO: Change return type to baked template when it's created.
		TypeInfo bake(std::vector<exec::CTV>& arg_list) const;

		CONSTRUCT_WITH_CHECKED_CAST(TemplateInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(TemplateInfo)
	};

	class TypeTemplateInfo: public TemplateInfo {
	public:
		SETUP_TYPE_WITH_BASE(TypeTemplateInfo, TemplateInfo)
		static TypeTemplateInfo create(std::vector<TypeDesc<>>& parameter_list);

		CONSTRUCT_WITH_CHECKED_CAST(TypeTemplateInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(TypeTemplateInfo)
	};
}
