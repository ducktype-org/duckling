#pragma once

#include "define_helper.hpp"
#include <utility>

namespace detail {
	template<typename ActionT>
	class DeferHelper {
		ActionT action;

	public:
		DeferHelper(ActionT &&action): action(std::move(action)) {}

		~DeferHelper() noexcept { action(); }
	};
}

/**
 * @brief Jai/Rift-like defer
 *
 * Defer takes any expression or code block and executes it after the block "ends"
 * during destruction of local variables.
 * Code inside defer should never throw.
 *
 * Multiple defers will execute in the reverse order of creation/scheduling:
 * defer (a);
 * defer (b);
 * here `b` will execute before `a`.
 * see: "Destruction sequence" in https://en.cppreference.com/w/cpp/language/destructor
 *
 * For technical reasons only one defer per line can be written.
 * Identifiers starting with `defer_custom_name_rJd7liva5_` should not be used when defer is used.
 *
 * See tests/test.cpp for examples.
 */
#define defer(code)                                                         \
	::detail::DeferHelper CONCAT_2(defer_custom_name_rJd7liva5_, __LINE__)( \
		[&]() noexcept -> void { code; }                                    \
	);
