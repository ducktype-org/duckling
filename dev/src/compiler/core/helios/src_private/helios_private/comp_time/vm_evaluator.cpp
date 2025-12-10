#include "vm_evaluator.hpp"

#include "ctv/ctv.hpp"
#include "helios_private/comp_time/comptime_type_operations.hpp"
#include "lir/lir_structure/lir_structure.hpp"
#include "typesystem/higher/kind.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <typesystem/higher/types.hpp>

#include "query_framework/context.hpp"

#include "vm/api/data/process_info.hpp"
#include "vm/bytecode/bytecode.hpp"
#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/thread/vmvalue.hpp>

#include <expected>

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
		variant_match(ctv.getStorage()) {
			variant_case(NumericValue, val) {
				return std::visit(
					[&](auto&& num_val) -> std::expected<Box<vm::VmValue>, VmEvaluationError> {
						using NumT = std::decay_t<decltype(num_val)>;

						// @TODO: #899 Once CTV will be VmValue based (contain the VMValue and
					    // compiler::tsh::SymbolType) we should perform this conversion based on the
					    // `SymbolType` not C++ type sizes.
						base::StrID dvm_type_name;
						if constexpr (sizeof(NumT) <= 2)
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

						auto vm_value_response = vm::api::getVmValue(pid, dvm_type_name.str());
						if (!vm_value_response.has_value())
							return std::unexpected(VmEvaluationError(
								VmEvaluationError::Kind::ArgConversionFailed,
								base::strConcat(
									"Failed to get VM value for '",
									dvm_type_name.strView(),
									"' type."
								)
							));
						auto res = std::move(vm_value_response->vm_value);
						res->writeBytes<NumT>(num_val);
						return res;
					},
					val.getStorage()
				);
			}
			variant_case(bool, val) {
				auto vm_value_response = vm::api::getVmValue(pid, "byte");
				if (!vm_value_response.has_value())
					return std::unexpected(VmEvaluationError(
						VmEvaluationError::Kind::ArgConversionFailed,
						"Failed to get VM value for 'byte'(bool) type."
					));

				auto res = std::move(vm_value_response->vm_value);
				res->writeBytes<bool>(val);
				return res;
			}
			variant_case(compiler::tsh::SymbolType<>, type) {
				auto vm_value_response = vm::api::getVmValue(pid, "opaque_ptr");
				if (!vm_value_response.has_value())
					return std::unexpected(VmEvaluationError(
						VmEvaluationError::Kind::ArgConversionFailed,
						"Failed to get VM value for 'opaque_ptr' type."
					));

				auto                               res = std::move(vm_value_response->vm_value);
				const compiler::tsh::SymbolType<>* type_ptr = &type;
				res->writeBytes<const compiler::tsh::SymbolType<>*>(type_ptr);
				return res;
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
			if (vm_value->type->getName() != base::StrID("byte"))
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
	 * @brief A helper class that manages the lifetime of the compile-time VM process.
	 * Spawns a new process when first used and kills it when compilation ends.
	 */
	class VmManager {
		base::Optional<vm::PID> pid{};
		// Code already added into this VMs process.
		vm::code::CodeCollection loaded_code;

	public:
		VmManager() {
			auto spawn_result = vm::api::spawn();
			if (spawn_result) pid = spawn_result->pid;
		}

		// @todo: Kill the CompTime VM process in the destructor once we get rid of the deadlock.
		// This should happen after #1222.
		~VmManager() = default;

		[[nodiscard]] base::Optional<vm::PID> getPID() const { return pid; }

		vm::code::CodeCollection& getLoadedCode() { return loaded_code; }

		void expandLoadedCode(vm::code::CodeCollection& new_code) {
			loaded_code.functions.insert(
				loaded_code.functions.end(), new_code.functions.begin(), new_code.functions.end()
			);
			loaded_code.types.insert(
				loaded_code.types.end(), new_code.types.begin(), new_code.types.end()
			);
			loaded_code.global_data.insert(
				loaded_code.global_data.end(),
				new_code.global_data.begin(),
				new_code.global_data.end()
			);
			loaded_code.external_c_functions.insert(
				loaded_code.external_c_functions.end(),
				new_code.external_c_functions.begin(),
				new_code.external_c_functions.end()
			);
		}

		VmManager(const VmManager&)            = delete;
		VmManager& operator=(const VmManager&) = delete;
	};

	template<typename T, typename KeyExtractor>
	std::vector<T> filterOutExisting(
		const std::vector<T>& source, const std::vector<T>& existing_items, KeyExtractor extractor
	) {
		auto existing_keys_view = existing_items | std::views::transform(extractor);
		std::unordered_set<base::StrID> existing_keys(
			existing_keys_view.begin(), existing_keys_view.end()
		);
		auto new_items_view = source | std::views::filter([&](const T& item) {
								  return !existing_keys.contains(extractor(item));
							  });
		return new_items_view | std::ranges::to<std::vector<T>>();
	}

	vm::code::CodeCollection filterCodeCollection(
		const vm::code::CodeCollection& code_to_load, const vm::code::CodeCollection& already_loaded
	) {
		auto by_name = [](const auto& item) -> base::StrID {
			using T = std::decay_t<decltype(item)>;
			if constexpr (std::is_same_v<T, vm::code::TypeOfData>)
				return vm::code::typeName(item);
			else
				return item.name;
		};

		return vm::code::CodeCollection{
			.functions
			= filterOutExisting(code_to_load.functions, already_loaded.functions, by_name),
			.types = filterOutExisting(code_to_load.types, already_loaded.types, by_name),
			.global_data
			= filterOutExisting(code_to_load.global_data, already_loaded.global_data, by_name),
			.external_c_functions = filterOutExisting(
				code_to_load.external_c_functions, already_loaded.external_c_functions, by_name
			)
		};
	}

	vm::code::CodeCollection produceCodeCollectionFromLIR(
		vm::PID pid, const std::vector<CRef<compiler::lir::Function>>& all_lir_functions
	) {
		compiler::backend_vm::Module m(base::StrID("COMP_TIME"));

		// TODOP: Move comptime func loading to constructor?
		auto comptime_definitions = comptime_ops::getComptimeTypeOperations(pid);
		m.insertRawBytecodeDefinitions(comptime_definitions);

		for (const auto& lir_function: all_lir_functions) m.insertLirFunction(lir_function);

		// TODOP: Inline m.build() once debug prints are removed.
		vm::code::CodeCollection code = m.build();

		std::cout << "Produced DVM bytecode:\n";
		vm::code::serialize(code, std::cout);
		std::cout << "\n";

		return code;
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
		// @note: vm_manager is initialized (spawns the DVM compile-time evaluation process)
		// once upon the first call to executeInVm and its lifetime extends for the duration of
		// the program. When deinitialized, it kills the spawned process.
		static VmManager vm_manager;
		auto             maybe_pid = vm_manager.getPID();
		if (!maybe_pid)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ProcessSpawnFailed, "Failed to spawn VM process."
			));
		vm::PID pid = maybe_pid.value();

		auto code = produceCodeCollectionFromLIR(pid, lir_functions);

		// @note: Remove functions/types/globals etc. that already exist in this VM instance
		// (from previous compile time evaluations). Inserting duplicate elements will cause the
		// whole code collection to be rejected.
		auto filtered_code = filterCodeCollection(code, vm_manager.getLoadedCode());
		auto load_result   = vm::api::loadCode(pid, { filtered_code });
		if (!load_result) {
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::CodeLoadFailed, "Failed to load code into VM."
			));
		}

		// If the code load succeeded, we expand the VMs manger context. If any of the functions
		// was rejected the internal VMs state won't be changed.
		vm_manager.expandLoadedCode(filtered_code);

		// TODOP: Split this. Probablby move to VMManager.
		// Pass the query context into DVM.
		auto response = vm::api::getVmValue(pid, "opaque_ptr");
		if (!response.has_value())
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ArgConversionFailed,
				"Failed to fetch an opaque pointer when evaluating '" + func_name + "' on DVM."
			));
		auto  ctx_vm_value      = std::move(response->vm_value);
		auto* query_context_ptr = &ctx;
		ctx_vm_value->writeBytes(query_context_ptr);

		if (auto res = vm::api::runFunction(pid, "__comptime_set_ctx", { ctx_vm_value.refMut() });
		    !res)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::FunctionRunFailed,
				"Failed to initialize the global context on DVM."
			));
		if (auto res = vm::api::join(pid); !res)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::VmJoinFailed, "Failed to join VM process."
			));


		std::vector<Box<vm::VmValue>> owned_arguments;
		owned_arguments.reserve(args.size());
		for (const auto& ctv_arg: args) {
			auto res = ctvToVmValue(pid, ctv_arg);
			if (!res) return std::unexpected(res.error());
			owned_arguments.push_back(std::move(*res));
		}

		vm::FunctionRunArguments vm_args
			= owned_arguments | std::views::transform([](auto& value) { return value.refMut(); })
		    | std::ranges::to<vm::FunctionRunArguments>();

		if (auto res = vm::api::runFunction(pid, func_name, vm_args); !res)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::FunctionRunFailed,
				"Failed to run a function '" + func_name + "' on VM."
			));
		if (auto res = vm::api::join(pid); !res)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::VmJoinFailed, "Failed to join VM process."
			));

		// Free the owned arguments.
		for (const auto& arg: owned_arguments) arg->freeData();

		auto exit_value = vm::api::getExitValue(pid);
		if (!exit_value)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::GetExitValueFailed,
				"Failed to get exit value from VM after function execution."
			));

		return vmValueToCtv(return_type, exit_value.value());
	}
}
