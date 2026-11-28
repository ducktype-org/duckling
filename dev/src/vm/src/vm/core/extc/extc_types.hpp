// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <expected>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace vm {
	class IVMValue;
	class ExtCContext;

	/** @brief Why an external C function failed. The VM turns it into a panic. */
	struct ExtCError final {
		std::string message;
	};

	/// Value convention: arguments, copies owned by the VM for the call's duration.
	using ExtCArgs = std::span<const CRef<IVMValue>>;

	/// Value convention: results, in the order of the function's result types.
	using ExtCResults = std::vector<Box<IVMValue>>;

	using ExtCReturn = std::expected<ExtCResults, ExtCError>;

	/// An external C function using the value convention, see `vm/core/extc/extc.hpp`.
	using ExtCValueFunctionPtr = ExtCReturn (*)(ExtCContext& ctx, ExtCArgs args);

	/// Raw convention: the bytes of each argument.
	using ExtCRawArgs = std::span<const byte* const>;

	/// Raw convention: where to write the bytes of each result.
	using ExtCRawResults = std::span<byte* const>;

	using ExtCRawReturn = std::expected<void, ExtCError>;

	/// An external C function using the raw convention, see `vm/core/extc/extc.hpp`.
	using ExtCRawFunctionPtr
		= ExtCRawReturn (*)(ExtCContext& ctx, ExtCRawArgs args, ExtCRawResults results);

	/// An external C function in either convention.
	using ExtCFunction = std::variant<ExtCRawFunctionPtr, ExtCValueFunctionPtr>;
}
