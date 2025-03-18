/**
 * @file typesystem.hpp
 * @brief Aggregates the interface of the Type System.
 */

#pragma once

#include "kind.hpp"
#include "abstract_type.hpp"
#include "symbol_type.hpp"
#include "expression_type.hpp"
#include "types.hpp"
#include "type_interface.hpp"

#include "queries.hpp"

/**
 * @brief The namespace of all definitions of the Higher Type System.
 * Short for "Type System: High(er)".
 */
namespace tsh {
	void reset();
}
