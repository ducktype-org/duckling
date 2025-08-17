#include "vm_evaluator.hpp"

#include <vm/api/vm.hpp>
#include <vm/core/thread/vmvalue.hpp>

namespace {
	// TODOP: This may be moved to a separate module in the future since it will get much more
	// complicated.
	std::expected<Box<vm::VmValue>, compiler::helios::errors::Failed> ctvToVmValue(
		vm::PID pid, const compiler::helios::CTV& ctv
	) {
		variant_match(ctv) {
			variant_case(i64, val) {
				auto vm_value_response = vm::api::getVmValue(pid, "i64");
				if (!vm_value_response.has_value()) {}
				auto res = std::move(vm_value_response->vm_value);
				res->writeBytes<i64>(val);
				return res;
			}
			variant_default {
				throw base::NotYetImplemented(
					"Conversion from ctv to VmValue for this type is not implemented yet"
				);
			}
		}
		// TODOP: Remove
		return std::unexpected(compiler::helios::errors::Failed());
	}

	// TODOP: Cast `vm_value` into `type`.
	std::expected<compiler::helios::CTV, compiler::helios::errors::Failed> vmValueToCtv(
		const tsh::SymbolType<>&, Ref<vm::VmValue> vm_value
	) {
		// std::cout << type.getType().toString() << '\n';
		// std::cout << type.toString() << '\n';
		// std::cout << base::enumToStr(type.getType().getKind()).str() << '\n';
		// // TODOP: Legit conversion using the return type.

		if (vm_value->type->getName() != base::StrID("i64")) {
			throw base::NotYetImplemented("Conversion from vmValue to CTV for types other than i64");
		}

		std::cout << "vmValue to CTV converting\n";
		i64 result = vm_value->readBytes<i64>();
		return result;
	}
}

namespace compiler::helios {
	CompileTimeEvaluator& CompileTimeEvaluator::get() {
		static CompileTimeEvaluator instance;
		return instance;
	}

	CompileTimeEvaluator::CompileTimeEvaluator() = default;

	std::expected<CTV, errors::Failed> CompileTimeEvaluator::executeInVm(
		const tsh::SymbolType<>&        return_type,
		const vm::code::CodeCollection& code,
		const std::string&              func_name,
		const std::vector<CTV>&         args
	) {
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
		// TODOP: Add timeouts to the VM in the future?
		if (auto res = vm::api::join(pid); !res) return std::unexpected(errors::Failed());

		// Free the owned arguments.
		for (const auto& arg: owned_arguments) arg->freeData();

		auto exit_value = vm::api::getExitValue(pid);
		if (!exit_value) return std::unexpected(errors::Failed());

		if (!exit_value.has_value()) CORE_PANIC("Empty VmValue response");
		// TODOP: Remove that.
		auto vm_value = exit_value.value();
		vm_value->dprint();

		std::cout << "Func return type: " << return_type.toString() << '\n';

		// TODOP: Maybe it would be nice if getExitValue() returned a Box as well so we could free it?
		// TODOP: Maybe add a separate endpoint for CompTimeGetExitValue() which returns the Box to
		// avoid memory bloat.
		auto ctv_res = vmValueToCtv(return_type, exit_value.value());

		std::cout << "Converted\n";

		// if (auto res = vm::api::kill(pid); !res) return std::unexpected(errors::Failed());
		std::cout << "Exited CTV conversion\n";

		return ctv_res;
	}
}
