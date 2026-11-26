// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "value_type.hpp"

#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/tsh/source_type_name.hpp>

namespace compiler::tsh {

	template<std::derived_from<AbstractType> ABSTRACT_TYPE>
	ValueType<ABSTRACT_TYPE> ValueType<ABSTRACT_TYPE>::getPointeeValueType() const {
		// If this is Ref or Box we want to get the underlying direct type.
		if (reference_kind == ReferenceKind::Ref || reference_kind == ReferenceKind::Box)
			return toDirect();
		// Now this has to be a pointer
		switch (abstract_type.getKind()) {
		case tsh::Kind::Pointer:
			return abstract_type.template as<tsh::PointerAbstractType>().getPointee();
		case tsh::Kind::ManyPointer:
			return abstract_type.template as<tsh::ManyPointerAbstractType>().getPointee();
		case tsh::Kind::CPointer:
			return abstract_type.template as<tsh::CPointerAbstractType>().getPointee();
		default:
			CORE_PANIC("Cannot dereference a non-pointer like type");
		}
	}

	template auto ValueType<AbstractType>::getPointeeValueType() const -> ValueType<AbstractType>;

	template<std::derived_from<AbstractType> ABSTRACT_TYPE>
	std::string ValueType<ABSTRACT_TYPE>::sourceTypeName(query::Context& ctx) const {
		return renderValueTypeName(ctx, ValueType<AbstractType>(*this)).text;
	}

	template std::string ValueType<AbstractType>::sourceTypeName(query::Context&) const;
}
