// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "extc_invoke.hpp"

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/thread/kill_process_exception.hpp>

namespace {
	template<class RETURN, class CALL>
	RETURN invokeCatching(CALL call) {
		try {
			return call();
		} catch (const vm::KillProcessException&) {
			throw;
		} catch (const vm::exceptions::VMRuntimeException& error) {
			return std::unexpected(vm::ExtCError{ error.what() });
		} catch (const std::exception& error) {
			return std::unexpected(vm::ExtCError{ base::strConcat("C++ exception: ", error.what()) }
			);
		} catch (...) { return std::unexpected(vm::ExtCError{ "unknown C++ exception" }); }
	}
}

vm::ExtCReturn vm::invokeExtCFunction(
	ExtCValueFunctionPtr function, ExtCContext& ctx, ExtCArgs args
) {
	return invokeCatching<ExtCReturn>([&] { return function(ctx, args); });
}

vm::ExtCRawReturn vm::invokeExtCFunction(
	ExtCRawFunctionPtr function, ExtCContext& ctx, ExtCRawArgs args, ExtCRawResults results
) {
	return invokeCatching<ExtCRawReturn>([&] { return function(ctx, args, results); });
}
