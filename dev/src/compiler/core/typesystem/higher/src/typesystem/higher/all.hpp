/**
 * @file typesystem.hpp
 * @brief Aggregates the interface of the Type System.
 */

#pragma once

#include "abstract_type.hpp"    // IWYU pragma: export
#include "expression_type.hpp"  // IWYU pragma: export
#include "kind.hpp"             // IWYU pragma: export
#include "queries.hpp"          // IWYU pragma: export
#include "symbol_type.hpp"      // IWYU pragma: export
#include "type_interface.hpp"   // IWYU pragma: export
#include "types.hpp"            // IWYU pragma: export

/**
 * @brief The namespace of all definitions of the Higher Type System.
 * Short for "Type System: High(er)".
 */
namespace tsh {
	void reset();
}
