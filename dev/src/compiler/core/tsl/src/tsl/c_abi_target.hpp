/**
 * @file c_abi_target.hpp
 *
 * @brief Single source of truth for the `TargetABI` value used by the
 * compiler when computing C-compatible class layouts.
 */
#pragma once

#include <abi/layout/target.hpp>

namespace compiler::tsl {

	/**
	 * @brief Returns the TargetABI used for C-layout computation in the compiler.
	 *
	 * The compiler currently produces code only for the host's default target
	 * (the LLVM backend also locks to it). When a target configuration is
	 * introduced, this helper becomes the only place that needs to change.
	 */
	inline abi::layout::TargetABI compilerTargetABI() { return abi::layout::x86_64Linux(); }

}
