#include "builders.hpp"
#include <backends/dvm/elements.hpp>
#include <backends/dvm/instructions.hpp>
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>
#include <code_data/opcode_args.hpp>
#include <base/ref.hpp>
#include <preprocessor/parser/types_of_data.hpp>
#include <ranges>
#include "utils.hpp"

compiler::backend_vm::Function compiler::backend_vm::FunctionBuilder::build() const {
	Function function;
	function.body = instructions;
	function.name = name;

	// @TODO: ret_size should be fixed somehow.
	function.stack_size    = max_stack_size;
	function.ret_size      = ret_size;
	function.arg_size      = 0;
	function.next_arg_size = 0;

	return function;
}

void compiler::backend_vm::CodeFileBuilder::addFunction(const FunctionBuilder& function) {
	functions.push_back(function);
}

void compiler::backend_vm::CodeFileBuilder::addType(const vm::TypeOfData& type) {
	if (type_map.atMaybe(typeName(type))) {
		// Type already exists, check if it's the same and if true, skip
	} else {
		auto [it, _] = type_map.put(typeName(type), type);
		types.emplace_back(&it->second);
	}
}

compiler::backend_vm::CodeFile compiler::backend_vm::CodeFileBuilder::build() const {
	auto file = CodeFile();
	for (auto&& tp: types) file.types.push_back(*tp);
	file.functions = std::ranges::to<std::deque<Function>>(
		functions | std::views::transform([&](const auto& function_builder) {
			return function_builder.build();
		})
	);
	return file;
}

void compiler::backend_vm::FunctionBuilder::addInstruction(const VmInstruction& instruction) {
	instructions.push_back(instruction);
}

compiler::backend_vm::FunctionBuilder::FunctionBuilder(
	base::StrID name, const base::HashMap<base::StrID, vm::TypeOfData>& available_types
):
	  name(name),
	  available_types(available_types) {}

const base::HashMap<base::StrID, vm::TypeOfData>&
	compiler::backend_vm::CodeFileBuilder::getAvailableTypes() const {
	return type_map;
}

usize getTypeSize(const vm::TypeOfData& tp) {
	variant_match(tp) {
		variant_case(vm::PrimitiveType, primitive) return primitive.size;

		NOIMPL_CASE(vm::PointerType, "get size of")
		NOIMPL_CASE(vm::StaticTableType, "get size of")
		NOIMPL_CASE(vm::DynamicTableType, "get size of")
		NOIMPL_CASE(vm::DataType, "get size of")
		NOIMPL_CASE(vm::VariantType, "get size of")
		NOIMPL_CASE(vm::FunctionType, "get size of")
	}
	CORE_UNREACHABLE();
}

usize compiler::backend_vm::FunctionBuilder::initType(base::StrID tp) {
	instructions.emplace_back(Op_init_type{ tp });

	const vm::TypeOfData& vm_type   = available_types[tp];
	const usize           type_size = getTypeSize(vm_type);
	usize                 offset    = 0;

	if (!local_stack.empty()) {
		const LocalStackEntry& prev_entry = local_stack.back();
		offset                            = prev_entry.local_stack_position + prev_entry.type_size;
	}

	local_stack.emplace_back(LocalStackEntry{
		.tp = tp, .local_stack_position = offset, .type_size = type_size });

	max_stack_size = std::max(max_stack_size, offset + type_size);

	return offset;
}

void compiler::backend_vm::FunctionBuilder::deinitType() {
	CORE_ASSERT(!local_stack.empty(), "Popping from empty variable stack");
	instructions.emplace_back(Op_deinit{});
	local_stack.pop_back();
}

usize compiler::backend_vm::FunctionBuilder::getLocalSize() const { return local_stack.size(); }

void compiler::backend_vm::InstructionBuilder::pushArg(const vm::opargs::OpCodeArg& arg) {
	args.push_back(arg);
}

void compiler::backend_vm::FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->instructions.push_back(instr);
}

compiler::backend_vm::InstructionBuilder::InstructionBuilder(OpKind kind) { setKind(kind); }
