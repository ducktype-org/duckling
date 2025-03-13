/**
 * @file typesystem.hpp
 * @brief Aggregates the interface of the Type System.
 */

#pragma once

#include "abstract_type.hpp"
#include "expression_type.hpp"
#include "kind.hpp"
#include "queries.hpp"
#include "type_interface.hpp"
#include "types.hpp"

/**
 * @brief The namespace of all definitions of the Higher Type System.
 * Short for "Type System: High(er)".
 */
namespace tsh {
	void init();  // if needed

	void reset();
}
