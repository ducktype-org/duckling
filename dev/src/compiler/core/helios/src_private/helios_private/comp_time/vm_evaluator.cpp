#include "vm_evaluator.hpp"

#include <vm/api/vm.hpp>
#include <vm/core/thread/vmvalue.hpp>

#include <expected>

namespace {
	using namespace compiler::helios;

	/**
	 * @brief Converts a given `ctv` to VmValue.
	 * @return The converted VmValue or errors::Failed if the conversion failed.
	 */
	std::expected<Box<vm::VmValue>, compiler::helios::errors::Failed> ctvToVmValue(
		vm::PID pid, const compiler::helios::CompileTimeValue& ctv
	) {
		variant_match(ctv.getStorage()) {
			variant_case(i64, val) {
				auto vm_value_response = vm::api::getVmValue(pid, "i64");
				if (!vm_value_response.has_value()) return std::unexpected(errors::Failed());

				auto res = std::move(vm_value_response->vm_value);
				res->writeBytes<i64>(val);
				return res;
			}
			variant_case(bool, val) {
				auto vm_value_response = vm::api::getVmValue(pid, "byte");
				if (!vm_value_response.has_value()) return std::unexpected(errors::Failed());

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
		return std::unexpected(errors::Failed());
	}

	/**
	 * @brief Converts a given `vm_value` to CTV representing a specified `type`.
	 * @return The converted value or errors::Failed if the conversion failed.
	 */
	std::expected<CompileTimeValue, compiler::helios::errors::Failed> vmValueToCtv(
		const tsh::SymbolType<>& type, Ref<vm::VmValue> vm_value
	) {
		const auto kind = type.getType().getKind();
		switch (kind) {
		case tsh::Kind::Integral: {
			if (vm_value->type->getName() != base::StrID("i64"))
				return std::unexpected(errors::Failed());
			return CompileTimeValue{ vm_value->readBytes<i64>() };
		}
		case tsh::Kind::Bool: {
			if (vm_value->type->getName() != base::StrID("bool"))
				return std::unexpected(errors::Failed());
			return CompileTimeValue{ vm_value->readBytes<bool>() };
		}
		default: {
			throw base::NotYetImplemented{ "VMValue to CTV conversion for type: "
				                           + base::enumToStr(kind).str()
				                           + " is not implemented yet." };
		}
		}
	}
}

namespace compiler::helios {
	CompileTimeEvaluator& CompileTimeEvaluator::get() {
		static CompileTimeEvaluator instance;
		return instance;
	}

	CompileTimeEvaluator::CompileTimeEvaluator() = default;

	std::expected<CompileTimeValue, errors::Failed> CompileTimeEvaluator::executeInVm(
		const tsh::SymbolType<>&             return_type,
		const vm::code::CodeCollection&      code,
		const std::string&                   func_name,
		const std::vector<CompileTimeValue>& args
	) {
		// TODOP: Add timeouts to the VM in the future?
		// TODOP: Maybe it would be nice if getExitValue() returned a Box as well so we could free it?
		// TODOP: Maybe add a separate endpoint for CompTimeGetExitValue() which returns the Box to
		// avoid memory bloat.

		auto spawn_result = vm::api::spawn();
		if (!spawn_result) return std::unexpected(errors::Failed());
		const vm::PID pid = spawn_result->pid;

		if (auto res = vm::api::loadCode(pid, { code }); !res)
			return std::unexpected(errors::Failed());

		std::vector<Box<vm::VmValue>> owned_arguments;
		owned_arguments.reserve(args.size());
		for (const auto& ctv_arg: args) {
			auto res = ctvToVmValue(pid, ctv_arg);
			if (!res) return std::unexpected(errors::Failed());
			owned_arguments.push_back(std::move(*res));
		}

		vm::FunctionRunArguments vm_args
			= owned_arguments | std::views::transform([](auto& value) { return value.refMut(); })
		    | std::ranges::to<vm::FunctionRunArguments>();

		if (auto res = vm::api::runFunction(pid, func_name, vm_args); !res)
			return std::unexpected(errors::Failed());
		if (auto res = vm::api::join(pid); !res) return std::unexpected(errors::Failed());

		// Free the owned arguments.
		for (const auto& arg: owned_arguments) arg->freeData();

		auto exit_value = vm::api::getExitValue(pid);
		if (!exit_value) return std::unexpected(errors::Failed());

		if (!exit_value.has_value()) CORE_PANIC("Empty VmValue response");
		auto ctv_res = vmValueToCtv(return_type, exit_value.value());

		return ctv_res;
	}
}
