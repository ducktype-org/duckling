#include "builders.hpp"
#include "vm/program/instructions.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>
#include <vm/program/opcode_args.hpp>
#include <base/ref.hpp>
#include <vm/program/type_of_data.hpp>
#include <ranges>

#define NOIMPL_CASE(tp, reason)                                                          \
	variant_case(tp, _) {                                                                \
		throw base::NotYetImplemented(                                                   \
			base::strConcat("Unsupported type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                               \
	}

vm::program::Function vm::program::builders::FunctionBuilder::build() const {
	Function function;
	function.body = instructions;
	function.name = name;

	// @TODO: ret_size should be fixed somehow.
	function.local_stack_size = max_stack_size;
	function.ret_size         = ret_size;
	function.arg_size         = 0;
	function.next_arg_size    = 0;

	return function;
}

void vm::program::builders::CodeFileBuilder::addFunction(const FunctionBuilder& function) {
	functions.push_back(function);
}

void vm::program::builders::CodeFileBuilder::addType(const vm::program::TypeOfData& type) {
	if (type_map.atMaybe(typeName(type))) {
		// Type already exists, check if it's the same and if true, skip
	} else {
		auto [it, _] = type_map.put(typeName(type), type);
		types.emplace_back(&it->second);
	}
}

vm::program::CodeFile vm::program::builders::CodeFileBuilder::build() const {
	auto file = CodeFile();
	for (auto&& tp: types) file.types.push_back(*tp);
	file.functions = std::ranges::to<std::vector<Function>>(
		functions | std::views::transform([&](const auto& function_builder) {
			return function_builder.build();
		})
	);
	return file;
}

void vm::program::builders::FunctionBuilder::addInstruction(const VmInstruction& instruction) {
	instructions.push_back(instruction);
}

vm::program::builders::FunctionBuilder::FunctionBuilder(
	base::StrID name, const base::HashMap<base::StrID, vm::program::TypeOfData>& available_types
):
	  name(name),
	  available_types(available_types) {}

const base::HashMap<base::StrID, vm::program::TypeOfData>&
	vm::program::builders::CodeFileBuilder::getAvailableTypes() const {
	return type_map;
}

usize getTypeSize(const vm::program::TypeOfData& tp) {
	variant_match(tp) {
		variant_case(vm::parser::PrimitiveType, primitive) return primitive.size;
		NOIMPL_CASE(vm::parser::PointerType, "get size of")
		NOIMPL_CASE(vm::parser::StaticTableType, "get size of")
		NOIMPL_CASE(vm::parser::DynamicTableType, "get size of")
		NOIMPL_CASE(vm::parser::DataType, "get size of")
		NOIMPL_CASE(vm::parser::VariantType, "get size of")
		NOIMPL_CASE(vm::parser::FunctionType, "get size of")
	}
	CORE_UNREACHABLE();
}

usize vm::program::builders::FunctionBuilder::initType(base::StrID tp) {
	instructions.emplace_back(instructions::Op_init_type{ tp });

	const vm::program::TypeOfData& vm_type   = available_types[tp];
	const usize                    type_size = getTypeSize(vm_type);
	usize                          offset    = 0;

	if (!local_stack.empty()) {
		const LocalStackEntry& prev_entry = local_stack.back();
		offset                            = prev_entry.local_stack_position + prev_entry.type_size;
	}

	local_stack.emplace_back(LocalStackEntry{
		.tp = tp, .local_stack_position = offset, .type_size = type_size });

	max_stack_size = std::max(max_stack_size, offset + type_size);

	return offset;
}

void vm::program::builders::FunctionBuilder::deinitType() {
	CORE_ASSERT(!local_stack.empty(), "Popping from empty variable stack");
	instructions.emplace_back(instructions::Op_deinit{});
	local_stack.pop_back();
}

usize vm::program::builders::FunctionBuilder::getLocalSize() const { return local_stack.size(); }

void vm::program::builders::InstructionBuilder::pushArg(const vm::opargs::OpCodeArg& arg) {
	args.push_back(arg);
}

void vm::program::builders::FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->instructions.push_back(instr);
}

vm::program::builders::InstructionBuilder::InstructionBuilder(OpKind kind) { setKind(kind); }
