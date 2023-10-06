/**
 * @file kind.hpp
 * @brief Type kind definition
 */

#pragma once

namespace ts {
	enum class Kind {
		Void,
		Byte,
		Bool,
		Char,
		Integral,
		Float,
		RawPointer,
		Pointer,
		Function,
		Enum,
		Flag,
		Optional,
		Tuple,
		Variant,
		Class,
		TypeTemplate,
		Namespace,
		CodeBlock,
		Module,
		Meta,  // The "type" type
		VTable
	};
}
