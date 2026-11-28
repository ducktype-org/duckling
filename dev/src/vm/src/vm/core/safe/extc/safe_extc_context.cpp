// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "safe_extc_context.hpp"

#include <base/extend_cpp/defer.hpp>

#include <vm/core/extc/extc_invoke.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>

#include <array>

namespace {
	/// Argument and result counts up to this size need no allocation in the raw convention.
	constexpr usize INLINE_RAW_SLOTS = 8;

	[[noreturn]] void panic(const vm::low::LowExternCFunction& function, std::string_view reason) {
		throw vm::exceptions::VMExtCFunctionPanic(base::strConcat("'", function.name, "': ", reason)
		);
	}

	/// The data pointers of `count` slots, in `inline_storage` when it fits.
	template<class POINTER>
	std::span<POINTER> slotPointers(
		vm::LocalSlot*                         slots,
		usize                                  count,
		std::array<POINTER, INLINE_RAW_SLOTS>& inline_storage,
		std::vector<POINTER>&                  heap_storage
	) {
		std::span<POINTER> pointers;
		if (count <= INLINE_RAW_SLOTS) {
			pointers = std::span(inline_storage.data(), count);
		} else {
			heap_storage.resize(count);
			pointers = std::span(heap_storage);
		}
		for (usize i = 0; i < count; i++) pointers[i] = slots[i].data;
		return pointers;
	}
}

vm::SafeExtCContext::SafeExtCContext(
	SafeVMProcess& process, const low::LowExternCFunction& function, bool raw
):
	  process(process),
	  function(function),
	  raw(raw) {}

void vm::SafeExtCContext::callRaw(
	SafeVMProcess& process, Frame* frame, const low::LowExternCFunction& function
) {
	const usize arg_count    = function.parameters.size();
	const usize result_count = function.result_types.size();
	LocalSlot*  first_arg    = frame->local_slot_stack_end - arg_count;
	LocalSlot*  first_result = first_arg - result_count;

	std::array<const byte*, INLINE_RAW_SLOTS> inline_args{};
	std::array<byte*, INLINE_RAW_SLOTS>       inline_results{};
	std::vector<const byte*>                  heap_args;
	std::vector<byte*>                        heap_results;
	const auto args    = slotPointers(first_arg, arg_count, inline_args, heap_args);
	const auto results = slotPointers(first_result, result_count, inline_results, heap_results);

	SafeExtCContext ctx(process, function, true);
	auto            returned
		= invokeExtCFunction(std::get<ExtCRawFunctionPtr>(function.function), ctx, args, results);
	if (!returned.has_value()) panic(function, returned.error().message);
}

void vm::SafeExtCContext::callValue(
	SafeVMProcess& process, Frame* frame, const low::LowExternCFunction& function
) {
	const usize arg_count    = function.parameters.size();
	const usize result_count = function.result_types.size();
	LocalSlot*  first_arg    = frame->local_slot_stack_end - arg_count;
	LocalSlot*  first_result = first_arg - result_count;

	SafeExtCContext ctx(process, function, false);

	std::vector<Box<SafeVMValue>> args;
	std::vector<CRef<IVMValue>>   arg_refs;
	args.reserve(arg_count);
	arg_refs.reserve(arg_count);
	defer(for (auto& arg : args) arg->freeData());
	for (usize i = 0; i < arg_count; i++) {
		args.push_back(process.createOwnedVMValue(first_arg[i]));
		arg_refs.emplace_back(args.back().get());
	}

	auto returned
		= invokeExtCFunction(std::get<ExtCValueFunctionPtr>(function.function), ctx, arg_refs);
	if (!returned.has_value()) panic(function, returned.error().message);

	auto& results = returned.value();
	// Runs while a panic unwinds too, so it must not throw: values that are not ours to free are
	// only dropped.
	defer(for (auto& result : results) {
		auto* safe = dynamic_cast<SafeVMValue*>(result.get());
		if (safe != nullptr && safe->getPID() == process.getPID() && !safe->pointer.isNull())
			safe->freeData();
	});
	if (results.size() != result_count)
		panic(
			function,
			base::strConcat("returned ", results.size(), " results, expected ", result_count)
		);

	for (usize i = 0; i < result_count; i++) {
		const auto* result = dynamic_cast<const SafeVMValue*>(results[i].get());
		if (result == nullptr || result->getPID() != process.getPID())
			panic(function, base::strConcat("result ", i, " does not belong to this process"));
		if (result->type.get() != function.result_types[i].get())
			panic(
				function,
				base::strConcat(
					"result ",
					i,
					" has type '",
					result->type->getName(),
					"', expected '",
					function.result_types[i]->getName(),
					"'"
				)
			);
		if (result->pointer.isNull()) panic(function, base::strConcat("result ", i, " was freed"));
		result->exportToSlot(first_result[i]);
	}
}

vm::PID vm::SafeExtCContext::getPID() const { return process.getPID(); }

base::StrID vm::SafeExtCContext::functionName() const { return function.name; }

void* vm::SafeExtCContext::userData() const { return function.user_data; }

std::expected<Box<vm::IVMValue>, vm::ExtCError> vm::SafeExtCContext::createResult(usize index) {
	if (raw)
		return std::unexpected(ExtCError{
			"a raw-convention function writes its results instead of creating them" });
	if (index >= function.result_types.size())
		return std::unexpected(ExtCError{ base::strConcat(
			"result ",
			index,
			" requested from a function with ",
			function.result_types.size(),
			" results"
		) });
	return Box<IVMValue>(process.createOwnedVMValue(function.result_types[index]));
}

std::expected<Box<vm::IVMValue>, vm::ExtCError> vm::SafeExtCContext::createValue(base::StrID vm_type
) {
	auto type = process.getLoadedProgram()->getTypes().atMaybe(vm_type);
	if (!type.has_value())
		return std::unexpected(ExtCError{ base::strConcat("unknown VM type '", vm_type, "'") });
	return Box<IVMValue>(process.createOwnedVMValue(type.value()));
}

void vm::SafeExtCContext::freeValue(Box<IVMValue>&& value) {
	Box<IVMValue> owned = std::move(value);
	auto*         safe  = dynamic_cast<SafeVMValue*>(owned.get());
	if (safe == nullptr || safe->getPID() != process.getPID())
		throw exceptions::VMValueMismatchException("the value belongs to another process");
	if (safe->pointer.isNull()) throw exceptions::VMDoubleFreeException();
	safe->freeData();
}
