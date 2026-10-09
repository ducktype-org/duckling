// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "vm_evaluator.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <backends/dvm/repl_lowering.hpp>
#include <ctv/ctv.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/comp_time/comptime_type_operations.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/queries.hpp>

#include <base/collections/stable_container.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>

#include <algorithm>
#include <expected>
#include <iostream>
#include <mutex>

namespace {
	using namespace compiler::helios;
	using namespace compiler::ctv;
	using namespace compiler;

	/**
	 * @brief A class representing a compile time DVM instance. Spawns a VMProcess when
	 * constructed and loads all the needed context for compile time evaluations into the process.
	 * This includes initializing the global query context pointer, injecting all extern C functions
	 * operating on meta types.
	 *
	 * Handles the deduplication of the code being loaded into the VM.
	 * Kills the VMProcess when compilation ends.
	 */
	class CompTimeDVM final {
		base::Optional<vm::PID> pid{};

		/**
		 * Code builder, uses repl for incremental loading - it automatically handles
		 * deduplication and only lowers new elements.
		 */
		backend_vm::ReplDVMCodeBuilder code_builder;

	public:
		/**
		 * @brief Creates a VM comp time instance, loads all of the needed code for compile time
		 * evaluations and initializes the global query context pointer needed for meta type compile
		 * time
		 * evaluations.
		 */
		CompTimeDVM(query::Context& ctx): code_builder(ctx, true) {
			if (auto res = vm::api::spawn()) {
				pid = res->pid;
				if (!initializeCompTimeOps()) {
					CORE_ASSERT(
						vm::api::kill(pid.value()), "Failed to kill a freshly spawned comp time DVM"
					);
					pid.reset();
				}
			}
		}

		CompTimeDVM(const CompTimeDVM&)            = delete;
		CompTimeDVM& operator=(const CompTimeDVM&) = delete;

		~CompTimeDVM() {
			if (!pid.has_value()) return;
			dumpOutput();
			// Run the global destructors and validate the memory state after the last evaluation
			// completed cleanly or force kill the process.
			(void) vm::api::deinitOrKill(pid.value());
		}

		/**
		 * @brief Prints everything the compile-time code has printed so far on `std::cerr`.
		 *
		 * The DVM buffers the output of a program instead of writing it out, so a `print` inside a
		 * compile-time evaluation would otherwise be lost. Draining that buffer onto `std::cerr`
		 * makes such prints visible without mixing them into the compiler's own standard output.
		 */
		void dumpOutput() {
			if (!pid.has_value()) return;
			auto response = vm::api::output(pid.value());
			if (!response.has_value() || response->output.empty()) return;

			// Replace each \n with "[comp-time] \n" to prefix each line of output with the
			// compile-time tag.

			response->output = "[comp-time] " + response->output;

			constexpr static std::string_view PREFIX = "[comp-time] ";
			std::string::size_type            pos    = 0;
			while ((pos = response->output.find('\n', pos)) != std::string::npos) {
				response->output.insert(pos + 1, PREFIX);
				pos += PREFIX.length() + 1;
			}
			std::cerr << response->output << "\n";
		}

		[[nodiscard]] base::Optional<vm::PID> getPID() const { return pid; }

		[[nodiscard]] bool isAlive() const { return pid.has_value(); }

		/**
		 * @brief Loads the bytecode into the VM. Skips duplicated symbols.
		 */
		std::expected<void, VmEvaluationError> loadCode(
			query::Context& ctx, const lir::LIRUnit& lir_code
		) {
			// The builder outlives every query, so the context must not stay set past this call.
			code_builder.setContext(ctx);
			defer(code_builder.invalidateContext());

			auto new_code = code_builder.insertLIRUnitAndCollectNewlyLoweredCode(lir_code);
			if (auto load_result = vm::api::loadCode(*pid, new_code); !load_result) {
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::CodeLoadFailed,
					base::strConcat(
						"Failed to load code into VM: ", vm::api::errorToString(load_result.error())
					)
				));
			}
			return {};
		}

	private:
		bool initializeCompTimeOps() {
			auto code = comptime_ops::getComptimeTypeOperations(*pid);
			code_builder.insertRawBytecodeDefinitions(code);
			if (vm::api::loadCode(*pid, code)) return true;
			return false;
		}
	};

	/**
	 * @brief Materializes an aggregate CTV (e.g. a char-slice or String) as an owned VMValue by
	 * letting the DVM backend lower it.
	 *
	 * We synthesize a function like `fun anon() -> T = {return ctv;}`
	 * in LIR and hand it to the incremental code builder. Lowering the `return` of a `LIRConstant`
	 * runs the exact same `CTVLowering` path used for regular code, which emits the the DVM code
	 * returning the VMValue with the ctv. The DVM backend may generate new globals in the
	 * process.
	 */
	std::expected<Ref<vm::IVMValue>, VmEvaluationError> vmValueFromCtvBackendLowering(
		query::Context&                    ctx,
		CompTimeDVM&                       comptime_dvm,
		const CompileTimeValue&            ctv,
		const compiler::tsh::SymbolType<>& value_type
	) {
		static std::atomic<u64> anon_counter{ 0 };

		vm::PID     pid        = *comptime_dvm.getPID();
		const auto& layout     = ctx.query<tsl::QuerySymbolTypeLayout>(value_type)->valueOrThrow();
		auto        layout_ref = CRef<tsl::TypeLayout>(&layout);

		// Build `fun anon() -> value_type = return {ctv};`.
		lir::Block entry_block;
		entry_block.terminator = lir::Instruction{
			lir::Operation::ReturnValue,
			{},
			{ lir::LIRValue{ lir::LIRConstant{ .value = ctv, .layout = layout_ref } } },
			{},
		};

		base::StableVector<lir::Block> blocks;
		blocks.emplaceBack(std::move(entry_block));
		lir::BlockRef entry_block_ref = blocks.last();

		base::StrID func_name
			= base::StrID(base::strConcat("comptime_ctv_arg_", anon_counter.fetch_add(1)));

		lir::Function func{
			.mangled_name       = func_name,
			.abi                = { lir::LIRAbi::DefaultAbi{} },
			.link_once          = false,
			.ignore_on_dvm      = false,
			.ignore_on_llvm     = true,
			.parameter_layouts  = {},
			.return_type_layout = layout_ref,
			.blocks             = std::move(blocks),
			.local_list         = {},
			.block_order        = { entry_block_ref },
			.metadata           = { .position = {}, .source_code_name = {} },
		};

		lir::LIRUnit unit;
		unit.lir_functions.emplace_back(&func);

		if (auto res = comptime_dvm.loadCode(ctx, unit); !res) return std::unexpected(res.error());

		auto maybe_exit_value = vm::api::runFunctionAwait(pid, func_name.str());
		if (!maybe_exit_value.has_value())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::FunctionRunFailed,
				base::strConcat(
					"Failed to run a function '",
					func_name,
					"' on VM. Reason: `",
					errorToString(maybe_exit_value.error()),
					"`."
				)
			));

		Ref<vm::IVMValue> returned = [&]() -> Ref<vm::IVMValue> {
			variant_match(maybe_exit_value.value()) {
				variant_case(std::vector<Ref<vm::IVMValue>>, values) {
					CORE_ASSERT(values.size() == 1, "Expected a single materialized CTV value.");
					return values.at(0);
				}
				variant_default {
					CORE_PANIC("Unexpected non-vector return value when materializing a CTV.");
				}
			}
			CORE_UNREACHABLE();
		}();

		return returned;
	}

	/**
	 * @brief Converts a given `ctv` to VMValue.
	 * @return The converted VMValue or a VmEvaluationError if the conversion failed.
	 */
	std::expected<std::variant<Box<vm::IVMValue>, CRef<vm::IVMValue>>, VmEvaluationError> ctvToVMValue(
		query::Context& ctx, CompTimeDVM& comptime_dvm, const CompileTimeValue& ctv
	) {
		auto get_vm_value
			= [&](base::StrID type_name) -> std::expected<Box<vm::IVMValue>, VmEvaluationError> {
			auto vm_value_response = vm::api::getVMValue(*comptime_dvm.getPID(), type_name.str());
			if (!vm_value_response.has_value())
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::ArgConversionFailed,
					base::strConcat("Failed to get VM value for '", type_name.strView(), "' type.")
				));
			return std::move(vm_value_response->vm_value);
		};

		variant_match(ctv.getStorage()) {
			variant_case(NumericValue, val) {
				return std::visit(
					[&](auto&& num_val) -> std::expected<Box<vm::IVMValue>, VmEvaluationError> {
						using NumT = std::decay_t<decltype(num_val)>;

						// @TODO: #899 Once CTV will be VMValue based (contain the VMValue and
					    // compiler::tsh::SymbolType) we should perform this conversion based on the
					    // `SymbolType` not C++ type sizes.
						base::StrID dvm_type_name;
						if constexpr (sizeof(NumT) <= 1)
							dvm_type_name = base::StrID("i8");
						else if (sizeof(NumT) <= 2)
							dvm_type_name = base::StrID("i16");
						else if constexpr (sizeof(NumT) <= 4)
							dvm_type_name = base::StrID("i32");
						else if constexpr (sizeof(NumT) <= 8)
							dvm_type_name = base::StrID("i64");
						else {
							throw base::NotYetImplemented(
								"Conversion from CTV to VMValue for bigger numeric sizes"
							);
						}

						auto maybe_vm_value = get_vm_value(dvm_type_name);
						if (!maybe_vm_value) return maybe_vm_value;
						(*maybe_vm_value)->writeBytes<NumT>(num_val);
						return maybe_vm_value;
					},
					val.getStorage()
				);
			}
			variant_case(bool, val) {
				auto maybe_vm_value = get_vm_value(base::StrID("i8"));
				if (!maybe_vm_value) return maybe_vm_value;
				(*maybe_vm_value)->writeBytes<bool>(val);
				return maybe_vm_value;
			}
			variant_case(compiler::tsh::SymbolType<>, type) {
				auto maybe_vm_value = get_vm_value(base::StrID("opaque_ptr"));
				if (!maybe_vm_value) return maybe_vm_value;
				(*maybe_vm_value)->writeBytes<const compiler::tsh::SymbolType<>*>(&type);
				return maybe_vm_value;
			}
			variant_case(compiler::ctv::CompileTimeValue::CharSliceValue, _) {
				return vmValueFromCtvBackendLowering(
					ctx,
					comptime_dvm,
					ctv,
					compiler::tsh::SymbolType<>::withDefaults(tsh::getCharSliceType(ctx))
				);
			}
			variant_case(compiler::ctv::CompileTimeValue::StringClassValue, _) {
				if (!tsh::isStringTypePresent(ctx))
					return std::unexpected(VmEvaluationError(
						VmEvaluationError::Kind::ArgConversionFailed,
						"String class type is not available in this compilation."
					));
				return vmValueFromCtvBackendLowering(
					ctx,
					comptime_dvm,
					ctv,
					compiler::tsh::SymbolType<>::withDefaults(tsh::getStringType(ctx))
				);
			}
			variant_case(CompileTimeValue::VMValue, vm_value) {
				return CRef<vm::IVMValue>(vm_value.val.get());
			}
			variant_default {
				throw base::NotYetImplemented(
					"Conversion from ctv to VMValue for this type is not implemented yet: "
					+ ctv.getTypeOfStoredValue(ctx).toString() + " " + ctv.toString()
				);
			}
		}
		return std::unexpected(VmEvaluationError(
			VmEvaluationError::Kind::ArgConversionFailed,
			"Unknown error during CompileTimeValue to VMValue conversion"
		));
	}

	/**
	 * @brief Reads the pointer+length pair backing a char-slice/String VMValue into a CTV string.
	 * @note Assumes `vm_value`'s type was already confirmed to be the char-slice or String type.
	 */
	base::RawView charBackedVMValueToCtv(Ref<vm::IVMValue> vm_value) {
		using namespace vm::interpreted_data_variant;
		auto data    = vm_value->readData<Data>().value();
		auto pointer = data.fields.at(0).value->readData<Pointer>().value();
		auto length  = data.fields.at(1).value->readData<Primitive>().value();
		auto content = (*pointer.referenced)->readData<Table>().value().asBytesView();
		CORE_ASSERT(content.size() >= length.value, "Invalid char slice comp-time data.");
		return { content.getBegin(), length.value };
	}

	/**
	 * @brief Converts a given `vm_value` to CTV representing a specified `type`.
	 * @return The converted value or a VmEvaluationError if the conversion failed.
	 */
	std::expected<CompileTimeValue, VmEvaluationError> vmValueToCtv(
		query::Context& ctx, const compiler::tsh::SymbolType<>& type, Ref<vm::IVMValue> vm_value
	) {
		const auto kind = type.getType().getKind();
		switch (kind) {
		case compiler::tsh::Kind::Integral: {
			compiler::tsh::IntegralAbstractType int_type(type.getType());
			auto                                bit_size     = int_type.getSize();
			base::StrID                         vm_type_name = vm_value->getType()->getName();

			if (int_type.getSignedness()
			    == compiler::tsh::IntegralAbstractType::Signedness::Signed) {
				if (bit_size <= Bits{ 16 } && vm_type_name == "i16")
					return CompileTimeValue{ NumericValue{ vm_value->readBytes<i16>() } };
				else if (bit_size <= Bits{ 32 } && vm_type_name == "i32")
					return CompileTimeValue{ NumericValue{ vm_value->readBytes<i32>() } };
				else if (bit_size <= Bits{ 64 } && vm_type_name == "i64")
					return CompileTimeValue{ NumericValue{ vm_value->readBytes<i64>() } };
			} else {
				if (bit_size <= Bits{ 16 } && vm_type_name == "i16")
					return CompileTimeValue{ NumericValue{ vm_value->readBytes<u16>() } };
				if (bit_size <= Bits{ 32 } && vm_type_name == "i32")
					return CompileTimeValue{ NumericValue{ vm_value->readBytes<u32>() } };
				if (bit_size <= Bits{ 64 } && vm_type_name == "i64")
					return CompileTimeValue{ NumericValue{ vm_value->readBytes<u64>() } };
			}

			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ReturnConversionFailed,
				"Expected an integer VM value (i16/i32/i64), but received: " + vm_type_name.str()
			));
		}
		case compiler::tsh::Kind::Float: {
			compiler::tsh::FloatAbstractType float_type(type.getType());
			auto                             bit_size     = float_type.getSize();
			base::StrID                      vm_type_name = vm_value->getType()->getName();

			if (bit_size <= Bits{ 32 } && vm_type_name == "i32")
				return CompileTimeValue{ NumericValue{ vm_value->readBytes<f32>() } };
			else if (bit_size <= Bits{ 64 } && vm_type_name == "i64")
				return CompileTimeValue{ NumericValue{ vm_value->readBytes<f64>() } };

			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ReturnConversionFailed,
				"Mismatched VM value type for float return. Expected size "
					+ base::toString(bit_size) + " bits, but got VM type: " + vm_type_name.str()
			));
		}

		case compiler::tsh::Kind::Bool: {
			if (vm_value->getType()->getName() != base::StrID("byte")
			    && vm_value->getType()->getName() != base::StrID("i8"))
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::ReturnConversionFailed,
					"Expected byte (bool) VM value but received type: "
						+ vm_value->getType()->getName().str()
				));
			return CompileTimeValue{ vm_value->readBytes<bool>() };
		}
		case compiler::tsh::Kind::Meta: {
			if (vm_value->getType()->getName() != base::StrID("opaque_ptr"))
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::ReturnConversionFailed,
					"Expected opaque pointer VM value but received type: "
						+ vm_value->getType()->getName().str()
				));
			auto* meta_ptr = vm_value->readBytes<compiler::tsh::SymbolType<>*>();
			return CompileTimeValue{ *meta_ptr };
		}
		case compiler::tsh::Kind::Slice: {
			auto char_slice_type = tsh::SymbolType<>::withDefaults(tsh::getCharSliceType(ctx));
			if (vm_value->getType()->getName()
			    == ctx.query<mangler::QueryMangledType>(char_slice_type)->valueOrThrow())
				return CompileTimeValue{ CompileTimeValue::CharSliceValue{
					base::StrID(charBackedVMValueToCtv(vm_value)) } };
			return CompileTimeValue{ CompileTimeValue::VMValue{ .val = vm_value, .type = type } };
		}
		case compiler::tsh::Kind::Class: {
			if (tsh::isStringTypePresent(ctx)) {
				auto string_type = tsh::SymbolType<>::withDefaults(tsh::getStringType(ctx));
				if (vm_value->getType()->getName()
				    == ctx.query<mangler::QueryMangledType>(string_type)->valueOrThrow())
					return CompileTimeValue{ CompileTimeValue::StringClassValue{
						base::StrID(charBackedVMValueToCtv(vm_value)) } };
			}
			return CompileTimeValue{ CompileTimeValue::VMValue{ .val = vm_value, .type = type } };
		}
		default: {
			return CompileTimeValue{ CompileTimeValue::VMValue{ .val = vm_value, .type = type } };
		}
		}
	}

	using VmArguments = std::vector<std::variant<Box<vm::IVMValue>, CRef<vm::IVMValue>>>;

	std::expected<VmArguments, VmEvaluationError> prepareArguments(
		query::Context&                                     ctx,
		CompTimeDVM&                                        comptime_dvm,
		const std::vector<compiler::ctv::CompileTimeValue>& args
	) {
		VmArguments result;
		result.reserve(args.size());
		for (const auto& ctv_arg: args) {
			auto vm_val = ctvToVMValue(ctx, comptime_dvm, ctv_arg);
			if (!vm_val) return std::unexpected(vm_val.error());
			result.push_back(std::move(vm_val.value()));
		}
		return result;
	}

	std::expected<void, VmEvaluationError> setQueryContext(
		CompTimeDVM& comptime_dvm, query::Context& ctx
	) {
		vm::PID pid = *comptime_dvm.getPID();
		// Pass the query context into DVM.
		auto response = vm::api::getVMValue(pid, "opaque_ptr");
		if (!response.has_value())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ArgConversionFailed,
				"Failed to fetch an opaque pointer when evaluating on DVM."
			));
		auto ctx_vm_value = std::move(response->vm_value);
		ctx_vm_value->writeBytes(&ctx);

		if (!vm::api::runFunctionAwait(pid, "comptime_set_ctx", { ctx_vm_value.refMut() }))
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::FunctionRunFailed,
				"Failed to initialize the global context on DVM."
			));

		ctx_vm_value->freeData();
		return {};
	}

	std::expected<compiler::ctv::CompileTimeValue, VmEvaluationError> runAndGetResult(
		query::Context&                   ctx,
		CompTimeDVM&                      comptime_dvm,
		const std::string&                func_name,
		const VmArguments&                vm_value_args,
		const compiler::tsh::SymbolType<> return_type
	) {
		vm::PID pid = *comptime_dvm.getPID();

		auto to_ref = [&](auto& value) {
			variant_match(value) {
				variant_case(Box<vm::IVMValue>, val) { return val.ref(); }
				variant_case(CRef<vm::IVMValue>, val) { return val; }
			}
			CORE_UNREACHABLE();
		};

		vm::FunctionRunArguments args = vm_value_args | std::views::transform(to_ref)
		                              | std::ranges::to<vm::FunctionRunArguments>();

		auto maybe_exit_value = vm::api::runFunctionAwait(pid, func_name, args);

		if (!maybe_exit_value.has_value())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::FunctionRunFailed,
				base::strConcat(
					"Failed to run a function '",
					func_name,
					"' on VM. Reason: `",
					errorToString(maybe_exit_value.error()),
					"`."
				)
			));

		// Free the owned arguments.
		for (auto& arg: vm_value_args) {
			v_if_matches(arg, Box<vm::IVMValue>, val) { (*val)->freeData(); }
		}

		variant_match(maybe_exit_value.value()) {
			variant_case(std::vector<Ref<vm::IVMValue>>, values) {
				CORE_ASSERT(
					values.size() == 1, "Compiler support for multiple values not implemented"
				);
				auto exit_value = values.at(0);
				return vmValueToCtv(ctx, return_type, exit_value);
			}
			variant_default { CORE_PANIC("Unexpected non-vector return value from VM"); }
		}
		CORE_UNREACHABLE();
	}
}

namespace compiler::helios {
	std::expected<ctv::CompileTimeValue, VmEvaluationError> executeInVm(
		query::Context&                           ctx,
		const std::string&                        func_name,
		const lir::LIRUnit&                       lir_unit,
		const std::vector<ctv::CompileTimeValue>& args,
		const tsh::SymbolType<>&                  return_type
	) {
		static std::mutex vm_evaluation_mutex;
		std::scoped_lock  lock(vm_evaluation_mutex);

		// @note: comptime_dvm is initialized (spawns the DVM compile-time evaluation process and
		// initializes it) once upon the first call to executeInVm and its lifetime extends for the
		// duration of the program. When deinitialized, it kills the spawned process.
		static CompTimeDVM comptime_dvm(ctx);

		if (!comptime_dvm.isAlive())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::VmInitializationFailed,
				"Failed to initialize the comptime DVM process."
			));

		// Show what the evaluated code printed, whether or not the evaluation itself succeeded.
		defer(comptime_dvm.dumpOutput());

		if (auto res = comptime_dvm.loadCode(ctx, lir_unit); !res)
			return std::unexpected(res.error());

		if (auto res = setQueryContext(comptime_dvm, ctx); !res)
			return std::unexpected(res.error());

		auto vm_value_args = prepareArguments(ctx, comptime_dvm, args);
		if (!vm_value_args) return std::unexpected(vm_value_args.error());

		return runAndGetResult(ctx, comptime_dvm, func_name, *vm_value_args, return_type);
	}
}
