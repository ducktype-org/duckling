#pragma once

#include <base/types/ok_bad.hpp>

namespace base {
	/**
	 * Utility type wraping the base::OkBad,
	 * in a way that forces the value to be checked. the caller to check it (to avoid silent failures).
	 *
	 * If status method is never called, the destructor will panic.
     *
     * @note This is mostly useful as a return type for functions that can fail, to force the caller to check the result.
	 */
	struct CheckedOkBad final {
	private:
		base::OkBad result;
		bool        checked = false;

	public:
		CheckedOkBad(base::OkBad result);
		CheckedOkBad(const CheckedOkBad&) = delete;
		CheckedOkBad(CheckedOkBad&&)      = delete;

		~CheckedOkBad();

		[[nodiscard]]
		base::OkBad status();
	};

}
