#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>

namespace std {
	template<class Key>
	struct hash;
}

namespace pst {
	/**
	 * @brief Unique ID for each PST element.
	 * It can be used as session-unstable PST element hash.
	 */
	STRONG_TYPEDEF_ID(PstID);
}

ID_STD_HASH(::pst::PstID);
