// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "symbol_type.hpp"

#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/types.hpp>

namespace compiler::tsh {

	template<std::derived_from<AbstractType> ABSTRACT_TYPE>
	SymbolType<ABSTRACT_TYPE> SymbolType<ABSTRACT_TYPE>::getPointeeSymbolType() const {
		// If this is Ref or Box we want to get the underlying direct type.
		if (reference_kind == ReferenceKind::Ref || reference_kind == ReferenceKind::Box)
			return withReferenceKind(ReferenceKind::Direct);
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

	template auto SymbolType<AbstractType>::getPointeeSymbolType() const
		-> SymbolType<AbstractType>;
}
