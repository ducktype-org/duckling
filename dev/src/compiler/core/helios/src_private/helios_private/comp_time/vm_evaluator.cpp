#include "vm_evaluator.hpp"

#include <typesystem/higher/types.hpp>

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
	// TODOP: Maybe revert this change, as theres no way to test it without support in backend.
	std::expected<Box<vm::VmValue>, VmEvaluationError> ctvToVmValue(
		vm::PID pid, const CompileTimeValue& ctv
	) {
		variant_match(ctv.getStorage()) {
			variant_case(NumericValue, val) {
				return std::visit(
					[&](auto&& num_val) -> std::expected<Box<vm::VmValue>, VmEvaluationError> {
						using NumT = std::decay_t<decltype(num_val)>;

						base::StrID dvm_type_name;
						if constexpr (sizeof(NumT) <= 2)
							dvm_type_name = base::StrID("i16");
						else if constexpr (sizeof(NumT) <= 4)
							dvm_type_name = base::StrID("i32");
						else if constexpr (sizeof(NumT) <= 8)
							dvm_type_name = base::StrID("i64");
						else if constexpr (sizeof(NumT) <= 16)
							dvm_type_name = base::StrID("i128");
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
		const tsh::SymbolType<>& type, Ref<vm::VmValue> vm_value
	) {
		const auto kind = type.getType().getKind();
		switch (kind) {
		case tsh::Kind::Integral: {
			tsh::IntegralAbstractType int_type(type.getType());
			auto                      bit_size     = int_type.getSize();
			base::StrID               vm_type_name = vm_value->type->getName();

			if (int_type.getSignedness() == tsh::IntegralAbstractType::Signedness::Signed) {
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
		case tsh::Kind::Float: {
			tsh::FloatAbstractType float_type(type.getType());
			auto                   bit_size     = float_type.getSize();
			base::StrID            vm_type_name = vm_value->type->getName();

			if (bit_size <= Bits{ 32 } && vm_type_name == "i32")
				return CompileTimeValue{ NumericValue{ vm_value->readBytes<f32>() } };
			else if (bit_size <= Bits{ 64 } && vm_type_name == "i64")
				return CompileTimeValue{ NumericValue{ vm_value->readBytes<f64>() } };
			else if (bit_size <= Bits{ 128 } && vm_type_name == "i128")
				return CompileTimeValue{ NumericValue{ vm_value->readBytes<f128>() } };

			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ReturnConversionFailed,
				"Mismatched VM value type for float return. Expected size "
					+ base::toString(bit_size) + " bits, but got VM type: " + vm_type_name.str()
			));
		}

		case tsh::Kind::Bool: {
			if (vm_value->type->getName() != base::StrID("byte"))
				return std::unexpected(VmEvaluationError(
					VmEvaluationError::Kind::ReturnConversionFailed,
					"Expected byte (bool) VM value but received type: "
						+ vm_value->type->getName().str()
				));
			return CompileTimeValue{ vm_value->readBytes<bool>() };
		}
		default: {
			throw base::NotYetImplemented{ "VMValue to CTV conversion for type: "
				                           + base::enumToStr(kind).str()
				                           + " is not implemented yet." };
		}
		}
	}

	/**
	 * @brief A helper class that manages the lifetime of the compile-time VM process.
	 * Spawns a new process when first used and kills it when compilation ends.
	 */
	class VmManager {
		base::Optional<vm::PID> pid{};

	public:
		VmManager() {
			auto spawn_result = vm::api::spawn();
			if (spawn_result) pid = spawn_result->pid;
		}

		// @todo: Kill the CompTime VM process in the destructor once we get rid of the deadlock.
		// This should happen after #1222.
		~VmManager() = default;

		[[nodiscard]] base::Optional<vm::PID> getPID() const { return pid; }

		VmManager(const VmManager&)            = delete;
		VmManager& operator=(const VmManager&) = delete;
	};
}

namespace compiler::helios {
	std::expected<ctv::CompileTimeValue, VmEvaluationError> executeInVm(
		const std::string&                        func_name,
		const vm::code::CodeCollection&           code,
		const std::vector<ctv::CompileTimeValue>& args,
		const tsh::SymbolType<>&                  return_type
	) {
		// @note: vm_manager is initialized (spawns the DVM compile-time evaluation process) once
		// upon the first call to executeInVm and its lifetime extends for the duration of the
		// program. When deinitialized, it kills the spawned process.
		static VmManager vm_manager;
		auto             maybe_pid = vm_manager.getPID();
		if (!maybe_pid)
			return std::unexpected(VmEvaluationError(
				VmEvaluationError::Kind::ProcessSpawnFailed, "Failed to spawn VM process."
			));
		vm::PID pid = maybe_pid.value();

		auto load_result = vm::api::loadCode(pid, { code });
		if (!load_result) {
			variant_match(load_result.error()) {
				variant_case(vm::api::LoadProgramError, load_error) {
					// @note: Since we use one process for all evaluations in the VM, if two
					// constant expressions call the same function we're gonna get a load error
					// because of the duplicated function.
					if (load_error.why.find(vm::code::DuplicatedFunctionError::ERR_MSG)
					    != std::string::npos) {
						return std::unexpected(VmEvaluationError(
							VmEvaluationError::Kind::CodeLoadFailed,
							"Failed to load code into VM: " + load_error.why
						));
					}
				}
				variant_default {
					return std::unexpected(VmEvaluationError(
						VmEvaluationError::Kind::CodeLoadFailed, "Failed to load code into VM."
					));
				}
			}
		}

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
