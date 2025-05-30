#pragma once

#include "../pst_id.hpp"

#include <query_framework/query_input.hpp>

namespace pst::detail {
	struct PSTAccessKey {
		PstID pst_id;

		PSTAccessKey(PstID pst_id): pst_id(pst_id) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return pst_id.asInt();
		}
	};

	/**
	 * @brief Query that is used as a PST-access side input.
	 * @note it is in hpp only to be able to get its QueryID.
	 */
	DECLARE_QUERY_SIDE_INPUT(PSTAccessSideInput, PSTAccessKey);
}
