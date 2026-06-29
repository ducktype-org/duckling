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
	 * The compiler currently produces code only for the host's target, so this
	 * resolves to `abi::layout::hostTargetABI()`.
	 */
	const abi::layout::TargetABI& compilerTargetABI();

}
