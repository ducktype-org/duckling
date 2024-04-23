/**
 * @file typesystem.hpp
 * @brief Aggregates the interface of the Type System.
 */

#pragma once

#include "kind.hpp"
#include "queries.hpp"
#include "type_desc.hpp"
#include "type_info.hpp"
#include "types.hpp"

namespace ts {
	void init();  // if needed

	void reset();
}
