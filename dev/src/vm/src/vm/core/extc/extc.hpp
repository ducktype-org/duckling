// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file extc.hpp
 * @brief External C functions: C++ functions registered in a VM process and called from bytecode
 * with `call_cfunc`.
 *
 * Each function uses one of two calling conventions (the hybrid ABI):
 *
 * - **raw** (`ExtCRawFunctionPtr`): the function reads the bytes of its arguments and writes the
 *   bytes of its results directly in the VM's variables. It is as cheap as a call can be, but only
 *   allows trivially copyable VM types (no VM pointers, no variants), which the loader checks. A
 *   result holds the caller's current value of its variable, so the function must write every
 *   result.
 * - **value** (`ExtCValueFunctionPtr`): the function gets its arguments as VMValues (copies owned
 *   by the VM, freed after the call) and returns one VMValue per result type, in order. Any VM type
 *   is allowed.
 *
 * ```cpp
 * vm::ExtCRawReturn add(vm::ExtCContext&, vm::ExtCRawArgs args, vm::ExtCRawResults results) {
 *     vm::safeWriteBytes(results[0], vm::safeReadPointerBytes<i64>(args[0])
 *                                    + vm::safeReadPointerBytes<i64>(args[1]));
 *     return {};
 * }
 * ```
 *
 * Most functions are written as plain C++ functions instead, and the convention is picked for
 * them, see `extc_function.hpp`.
 *
 * Returning an `ExtCError`, or throwing any exception, makes the calling VM thread panic. Only the
 * exceptions that end the process pass through unchanged.
 *
 * @note The interface is the same for every VM implementation, so a function must not assume a
 * concrete VMValue class.
 */
#pragma once

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <vm/api/data/process_info.hpp>
#include <vm/core/extc/extc_types.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>

#include <expected>

namespace vm {
	/**
	 * @brief What an external C function can do with the VM during its call.
	 * @note Only valid during the call it was passed to.
	 */
	class ExtCContext {
	public:
		ExtCContext()                              = default;
		ExtCContext(const ExtCContext&)            = delete;
		ExtCContext(ExtCContext&&)                 = delete;
		ExtCContext& operator=(const ExtCContext&) = delete;
		ExtCContext& operator=(ExtCContext&&)      = delete;
		virtual ~ExtCContext()                     = default;

		/** @brief The process the function is called in. */
		[[nodiscard]] virtual PID getPID() const = 0;

		/** @brief The name the function was registered under. */
		[[nodiscard]] virtual base::StrID functionName() const = 0;

		/** @brief The user data the function was registered with. */
		[[nodiscard]] virtual void* userData() const = 0;

		template<class T>
		[[nodiscard]] T& userData() const {
			return *static_cast<T*>(userData());
		}

		/**
		 * @brief Creates a zeroed value for the `index`-th result of the called function. Only in
		 * the value convention.
		 * @note The value belongs to the function until it is returned or freed with `freeValue`.
		 */
		[[nodiscard]] virtual std::expected<Box<IVMValue>, ExtCError> createResult(usize index) = 0;

		/**
		 * @brief Creates a zeroed value of the VM type named `vm_type`.
		 * @note The value belongs to the function until it is returned or freed with `freeValue`.
		 */
		[[nodiscard]] virtual std::expected<Box<IVMValue>, ExtCError> createValue(base::StrID vm_type
		) = 0;

		/** @brief Frees a value created with `createResult` or `createValue`. */
		virtual void freeValue(Box<IVMValue>&& value) = 0;
	};
}
