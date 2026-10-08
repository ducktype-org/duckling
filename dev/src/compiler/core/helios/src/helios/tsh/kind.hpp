// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file kind.hpp
 * @brief Type kind definition
 */

#pragma once
#include <base/extend_cpp/stringifyable_enum.hpp>

// Doc style is intentional, caused by inexplicable funkiness in how Doxygen interacts with macros.
MAKE_STRINGIFYABLE_ENUM(compiler::tsh, u32, Kind
	/**
		@brief Enum which identifies the features of a type described in the Type System.
	*//**
		For example, the Void type does not hold much information about itself.
		However, each of the several Integral types holds a signedness boolean.
		Furthermore, a Tuple type holds information about its component types.
		Each class of types is described with a different Kind.
	*/,

	Unit,
	Void,
	Byte,
	Bool,
	Char,
	Integral,
	Float,
	RawPointer,
	Pointer,
	ManyPointer,
	CPointer,
	Slice,
	Function,
	StaticArray,
	Tuple,
	Variant,
	Class,
	TypeTemplate,
	Namespace,
	Module,

	/** @brief The kind of the import value. */
	Import,

	/** @brief The kind of the type which holds type values. In other words, the "type" type. */
	Meta
)

namespace compiler::tsh {
	inline bool isPointerKind(const Kind kind) {
		return kind == Kind::RawPointer || kind == Kind::Pointer || kind == Kind::ManyPointer
		    || kind == Kind::CPointer;
	}

	inline bool isIndexable(Kind kind) {
		return kind == Kind::Slice || kind == Kind::StaticArray || kind == Kind::ManyPointer
		    || kind == Kind::CPointer;
	}
}
