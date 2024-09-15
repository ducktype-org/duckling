/**
 * @file typesystem.hpp
 * @brief Aggregates the interface of the Type System.
 */

#pragma once

#include "kind.hpp"
#include "type_desc.hpp"
#include "type_info.hpp"
#include "types.hpp"
#include "type_interface.hpp"

#include "queries.hpp"

namespace tsh {
	void init();  // if needed

	void reset();
}
