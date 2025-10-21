#pragma once

#include <base/except/exceptions.hpp>

#include <query_framework/query_hash.hpp>
#include <query_framework/query_input.hpp>

namespace pst::internal {
	struct PSTAccessKey final {
		query::QueryStableHash hash;

		PSTAccessKey(query::QueryStableHash stable_hash): hash(stable_hash) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			CORE_PANIC("Unstable hash should not be used for PST access side input");
		}

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const {
			return hash;
		}
	};

	/**
	 * @brief Query that is used as a PST-access side input.
	 * @note it is in hpp only to be able to get its QueryID.
	 */
	DECLARE_QUERY_SIDE_INPUT(PSTAccessSideInput, PSTAccessKey);
}
