/**
 * @file typesystem.hpp
 * @brief Aggregates the interface of the Type System.
 */

#pragma once

#include "kind.hpp"
#include "type_desc.hpp"
#include "abstract_type.hpp"
#include "types.hpp"
#include "type_interface.hpp"

#include "queries.hpp"

/**
 * @brief The namespace of all definitions of the Higher Type System.
 * Short for "Type System: High(er)".
 */
namespace tsh {
	void init();  // if needed

	void reset();
}
