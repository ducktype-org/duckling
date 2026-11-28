// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/core/extc/extc.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/memory/frame.hpp>

namespace vm {
	class SafeVMProcess;

	/** @brief The safe VM's `ExtCContext`. */
	class SafeExtCContext final: public ExtCContext {
	private:
		SafeVMProcess&                 process;
		const low::LowExternCFunction& function;
		/// Whether the function uses the raw convention, which has no result values.
		bool raw;

		SafeExtCContext(SafeVMProcess& process, const low::LowExternCFunction& function, bool raw);

	public:
		/**
		 * @brief Calls a raw-convention `function` with the arguments in the topmost slots of
		 * `frame`, letting it write its results into the slots right below them. The argument
		 * slots are left to the caller.
		 * @throws exceptions::VMExtCFunctionPanic when the function fails.
		 */
		[[gnu::noinline]] static void callRaw(
			SafeVMProcess& process, Frame* frame, const low::LowExternCFunction& function
		);

		/**
		 * @brief Like `callRaw`, for a value-convention `function`: it gets copies of the
		 * arguments, and its results are copied into the result slots.
		 * @throws exceptions::VMExtCFunctionPanic when the function fails.
		 */
		[[gnu::noinline]] static void callValue(
			SafeVMProcess& process, Frame* frame, const low::LowExternCFunction& function
		);

		[[nodiscard]] PID getPID() const override;

		[[nodiscard]] base::StrID functionName() const override;

		[[nodiscard]] void* userData() const override;

		[[nodiscard]] std::expected<Box<IVMValue>, ExtCError> createResult(usize index) override;

		[[nodiscard]] std::expected<Box<IVMValue>, ExtCError> createValue(base::StrID vm_type
		) override;

		void freeValue(Box<IVMValue>&& value) override;
	};
}
