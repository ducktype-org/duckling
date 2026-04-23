#include "vm_evaluator.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/comp_time/comptime_type_operations.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>

#include <expected>
#include <mutex>

namespace {
	using namespace compiler::helios;
	using namespace compiler::ctv;

	/**
	 * @brief Converts a given `ctv` to VmValue.
	 * @return The converted VmValue or a VmEvaluationError if the conversion failed.
	 */
	std::expected<Box<vm::VmValue>, VmEvaluationError> ctvToVmValue(
		vm::PID pid, const CompileTimeValue& ctv
	) {
		auto get_vm_value
			= [&](base::StrID type_name) -> std::expected<Box<vm::VmValue>, VmEvaluationError> {
			auto vm_value_response = vm::api::getVmValue(pid, type_name.str());
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
					[&](auto&& num_val) -> std::expected<Box<vm::VmValue>, VmEvaluationError> {
						using NumT = std::decay_t<decltype(num_val)>;

						// @TODO: #899 Once CTV will be VmValue based (contain the VMValue and
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
								"Conversion from CTV to VmValue for bigger numeric sizes"
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
			variant_default {
				throw base::NotYetImplemented(
					"Conversion from ctv to VmValue for this type is not implemented yet"
				);
			}
		}
		return std::unexpected(VmEvaluationError(
			VmEvaluationError::Kind::ArgConversionFailed,
			"Unknown error during CompileTimeValue to VmValue conversion"
		));
	}

	/**
	 * @brief Converts a given `vm_value` to CTV representing a specified `type`.
	 * @return The converted value or a VmEvaluationError if the conversion failed.
	 */
	std::expected<CompileTimeValue, VmEvaluationError> vmValueToCtv(
		const compiler::tsh::SymbolType<>& type, Ref<vm::VmValue> vm_value
	) {
		const auto kind = type.getType().getKind();
		switch (kind) {
		case compiler::tsh::Kind::Integral: {
			compiler::tsh::IntegralAbstractType int_type(type.getType());
			auto                                bit_size     = int_type.getSize();
			base::StrID                         vm_type_name = vm_value->type->getName();

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
			base::StrID                      vm_type_name = vm_value->type->getName();

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
			if (vm_value->type->getName() != base::StrID("byte")
			    && vm_value->type->getName() != base::StrID("i8"))
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::ReturnConversionFailed,
					"Expected byte (bool) VM value but received type: "
						+ vm_value->type->getName().str()
				));
			return CompileTimeValue{ vm_value->readBytes<bool>() };
		}
		case compiler::tsh::Kind::Meta: {
			if (vm_value->type->getName() != base::StrID("opaque_ptr"))
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::ReturnConversionFailed,
					"Expected opaque pointer VM value but received type: "
						+ vm_value->type->getName().str()
				));
			auto* meta_ptr = vm_value->readBytes<compiler::tsh::SymbolType<>*>();
			return CompileTimeValue{ *meta_ptr };
		}
		default: {
			throw base::NotYetImplemented{ base::strConcat(
				"VMValue to CTV conversion for type: ",
				base::enumToStr(kind),
				" is not implemented yet."
			) };
		}
		}
	}

	/**
	 * @brief A class representing a compile time DVM instance. Spawns a VMProcess when
	 * constructed and loads all the needed context for compile time evaluations into the process.
	 * This includes initializing the global query context pointer, injecting all extern C functions
	 * operating on meta types.
	 *
	 * Handles the deduplication of the code being loaded into the VM.
	 * Kills the VMProcess when compilation ends.
	 */
	class CompTimeDVM {
		base::Optional<vm::PID> pid{};
		/**
		 * @brief Code already loaded into this VMs process.
		 * @note This set operates on mangled names, thus theres no need to distinguish between
		 * functions, types, globals, etc.
		 */
		std::unordered_set<base::StrID> loaded_symbols;

	public:
		/**
		 * @brief Creates a VM comp time instance, loads all of the needed code for compile time
		 * evaluations and initializes the global context pointer needed for meta type compile time
		 * evaluations.
		 */
		CompTimeDVM() {
			if (auto res = vm::api::spawn()) {
				pid = res->pid;
				if (!initializeCompTimeOps()) pid.reset();
			}
		}

		CompTimeDVM(const CompTimeDVM&)            = delete;
		CompTimeDVM& operator=(const CompTimeDVM&) = delete;

		// @TODO: #1222 Kill the CompTime VM process in the destructor once we get rid of the
		// deadlock.
		~CompTimeDVM() = default;

		[[nodiscard]] base::Optional<vm::PID> getPID() const { return pid; }

		[[nodiscard]] bool isAlive() const { return pid.has_value(); }

		/**
		 * @brief Loads the bytecode into the VM. Skips duplicated symbols.
		 */
		std::expected<void, VmEvaluationError> loadCode(const vm::code::CodeCollection& code) {
			auto new_code = filterOutLoaded(code);
			if (!vm::api::loadCode(*pid, new_code)) {
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::CodeLoadFailed, "Failed to load code into VM."
				));
			}
			markAsLoaded(new_code);
			return {};
		}

	private:
		bool initializeCompTimeOps() {
			auto code = comptime_ops::getComptimeTypeOperations(*pid);
			if (vm::api::loadCode(*pid, code)) {
				markAsLoaded(code);
				return true;
			}
			return false;
		}

		void markAsLoaded(vm::code::CodeCollection& loaded) {
			for (const auto& f: loaded.functions) loaded_symbols.insert(f.name);
			for (const auto& t: loaded.types) loaded_symbols.insert(vm::code::typeName(t));
			for (const auto& g: loaded.global_data) loaded_symbols.insert(g.name);
			for (const auto& e: loaded.external_c_functions) loaded_symbols.insert(e.name);
		}

		vm::code::CodeCollection filterOutLoaded(const vm::code::CodeCollection& code) {
			vm::code::CodeCollection filtered;
			auto                     by_name = [](const auto& item) -> base::StrID {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, vm::code::TypeOfData>)
                    return vm::code::typeName(item);
                else
                    return item.name;
			};

			auto insert_if_new = [&](const auto& source, auto& destination, auto name_getter) {
				for (const auto& item: source)
					if (!loaded_symbols.contains(name_getter(item))) destination.push_back(item);
			};

			insert_if_new(code.functions, filtered.functions, by_name);
			insert_if_new(code.types, filtered.types, by_name);
			insert_if_new(code.global_data, filtered.global_data, by_name);
			insert_if_new(code.external_c_functions, filtered.external_c_functions, by_name);
			return filtered;
		}
	};

	std::expected<void, VmEvaluationError> loadLirFunctions(
		CompTimeDVM&                                      comptime_dvm,
		const std::vector<CRef<compiler::lir::Function>>& all_lir_functions,
		query::Context&                                   query_ctx
	) {
		compiler::backend_vm::DVMCodeBuilder m(query_ctx, false);

		// Insert comptime context intto the module, for the module to pass the validation. This code
		// although loaded here multiple times will be deduplicated by `CompTimeDVM::loadCode()`
		auto comptime_code = comptime_ops::getComptimeTypeOperations(*comptime_dvm.getPID());
		m.insertRawBytecodeDefinitions(comptime_code);

		for (const auto& lir_function: all_lir_functions) m.insertLirFunction(lir_function);
		auto bytecode = m.build();
		return comptime_dvm.loadCode(bytecode);
	}

	std::expected<std::vector<Box<vm::VmValue>>, VmEvaluationError> prepareArguments(
		CompTimeDVM& comptime_dvm, const std::vector<compiler::ctv::CompileTimeValue>& args
	) {
		std::vector<Box<vm::VmValue>> owned_arguments;
		owned_arguments.reserve(args.size());
		for (const auto& ctv_arg: args) {
			auto res = ctvToVmValue(*comptime_dvm.getPID(), ctv_arg);
			if (!res) return std::unexpected(res.error());
			owned_arguments.push_back(std::move(*res));
		}
		return owned_arguments;
	}

	std::expected<void, VmEvaluationError> setQueryContext(
		CompTimeDVM& comptime_dvm, query::Context& ctx
	) {
		vm::PID pid = *comptime_dvm.getPID();
		// Pass the query context into DVM.
		auto response = vm::api::getVmValue(pid, "opaque_ptr");
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
		CompTimeDVM&                         comptime_dvm,
		const std::string&                   func_name,
		const std::vector<Box<vm::VmValue>>& owned_args,
		const compiler::tsh::SymbolType<>    return_type
	) {
		vm::PID pid = *comptime_dvm.getPID();

		vm::FunctionRunArguments args
			= owned_args | std::views::transform([](auto& value) { return value.refMut(); })
		    | std::ranges::to<vm::FunctionRunArguments>();

		auto maybe_exit_value = vm::api::runFunctionAwait(pid, func_name, args);

		if (!maybe_exit_value.has_value())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::FunctionRunFailed,
				"Failed to run a function '" + func_name + "' on VM."
			));

		// Free the owned arguments.
		for (const auto& arg: owned_args) arg->freeData();

		CORE_ASSERT(
			maybe_exit_value.value().size() == 1,
			"Compiler support for multiple values not implemented"
		);
		auto exit_value = maybe_exit_value.value().at(0);
		return vmValueToCtv(return_type, exit_value);
	}
}

namespace compiler::helios {
	std::expected<ctv::CompileTimeValue, VmEvaluationError> executeInVm(
		query::Context&                           ctx,
		const std::string&                        func_name,
		const std::vector<CRef<lir::Function>>&   lir_functions,
		const std::vector<ctv::CompileTimeValue>& args,
		const tsh::SymbolType<>&                  return_type
	) {
		static std::mutex vm_evaluation_mutex;
		std::scoped_lock  lock(vm_evaluation_mutex);

		// @note: comptime_dvm is initialized (spawns the DVM compile-time evaluation process and
		// initializes it) once upon the first call to executeInVm and its lifetime extends for the
		// duration of the program. When deinitialized, it kills the spawned process.
		static CompTimeDVM comptime_dvm;
		if (!comptime_dvm.isAlive())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::VmInitializationFailed,
				"Failed to initialize the comptime DVM process."
			));

		if (auto res = loadLirFunctions(comptime_dvm, lir_functions, ctx); !res)
			return std::unexpected(res.error());

		if (auto res = setQueryContext(comptime_dvm, ctx); !res)
			return std::unexpected(res.error());

		auto owned_args = prepareArguments(comptime_dvm, args);
		if (!owned_args) return std::unexpected(owned_args.error());

		return runAndGetResult(comptime_dvm, func_name, *owned_args, return_type);
	}
}
