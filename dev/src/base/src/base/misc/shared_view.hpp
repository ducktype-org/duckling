// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file shared_view.hpp
 * @brief Provides shared array view.
 * Separate from `raw_view.hpp` because of includes cycle.
 */

#pragma once

#include <base/misc/raw_view.hpp>
#include <base/pointers/shared_box.hpp>

namespace base {
	/**
	 * @brief Shared immutable byte array view
	 */
	class SharedView final {
		SharedBox<OwningView> content;

	public:
		SharedView(const SharedView&) = default;
		SharedView(SharedView&&)      = default;

		SharedView& operator=(const SharedView&) = default;
		SharedView& operator=(SharedView&&)      = default;

		/**
		 * @note Takes ownership of SharedBox
		 */
		explicit SharedView(SharedBox<OwningView> content): content(std::move(content)) {}

		/**
		 * @note Takes ownership, begin should be on heap.
		 */
		SharedView(byte* begin, usize size): content(makeSharedBox<OwningView>(begin, size)) {}

		SharedView static copy(RawView view) {
			return SharedView(makeSharedBox<OwningView>(OwningView::copy(view)));
		}

		// Makes copy
		explicit SharedView(const char* const c_str): content(makeSharedBox<OwningView>(c_str)) {}

		[[nodiscard]]
		const RawView view() const;
	};
}
