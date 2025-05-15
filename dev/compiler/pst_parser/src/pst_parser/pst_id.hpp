#pragma once

#include <base/strongly_typed_id.hpp>

#include <memory>

namespace pst {
	/**
	 * @brief Unique ID for each PST element.
	 * It can be used as session-unstable PST element hash.
	 */
	STRONG_TYPEDEF_ID(PstID);
}


ID_STD_HASH(::pst::PstID);