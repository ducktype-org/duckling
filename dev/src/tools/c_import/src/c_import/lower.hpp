/**
 * @file lower.hpp
 * @brief Decides what every C declaration becomes in Duckling.
 */
#pragma once

#include "c_model.hpp"
#include "dk_model.hpp"

#include <string>
#include <vector>

namespace c_import {

	struct LowerOptions final {
		/// Name globs a declaration of the requested headers has to match; empty means all.
		std::vector<std::string> include;
		/// Name globs that drop a declaration even when it matches `include`.
		std::vector<std::string> exclude;
	};

	/**
	 * @brief Lowers the declarations of the requested headers, plus the records they need.
	 *
	 * A record Duckling can lay out itself becomes an `extern("C") class` with its fields. Any
	 * other complete record (a union, a packed or bitfield record, one with an anonymous union
	 * member) becomes a layout blob: a class holding only correctly sized and aligned storage,
	 * reached through generated view and accessor functions. A record only ever used through a
	 * pointer becomes an opaque handle. Whatever cannot be lowered is listed in
	 * `DkModule::skipped` with the reason.
	 */
	DkModule lower(const CModel& model, const LowerOptions& options);

}
