#include "builders.hpp"
#include "vm/code/instructions.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>
#include <vm/code/opcode_args.hpp>
#include <base/ref.hpp>
#include <vm/code/type_of_data.hpp>
#include <ranges>

#define NOIMPL_CASE(tp, reason)                                                          \
	variant_case(tp, _) {                                                                \
		throw base::NotYetImplemented(                                                   \
			base::strConcat("Unsupported type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                               \
	}

vm::code::Function vm::code::builders::FunctionBuilder::build() const {
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

void vm::code::builders::CodeFileBuilder::addFunction(const FunctionBuilder& function) {
	functions.push_back(function);
}

void vm::code::builders::CodeFileBuilder::addType(const vm::code::TypeOfData& type) {
	if (type_map.atMaybe(typeName(type))) {
		// Type already exists, check if it's the same and if true, skip
	} else {
		auto [it, _] = type_map.put(typeName(type), type);
		types.emplace_back(&it->second);
	}
}

vm::code::CodeFile vm::code::builders::CodeFileBuilder::build() const {
	auto file = CodeFile();
	for (auto&& tp: types) file.types.push_back(*tp);
	file.functions = std::ranges::to<std::vector<Function>>(
		functions | std::views::transform([&](const auto& function_builder) {
			return function_builder.build();
		})
	);
	return file;
}

void vm::code::builders::FunctionBuilder::addInstruction(const Instruction& instruction) {
	variant_match(instruction) {
		variant_case(vm::code::instructions::Op_init_type, instr) {
			initType(instr.arg0.type_name);
		}
		variant_case(vm::code::instructions::Op_deinit, instr) { deinitType(); }
		variant_default instructions.push_back(instruction);
	}
}

vm::code::builders::FunctionBuilder::FunctionBuilder(
	base::StrID name, const base::HashMap<base::StrID, vm::code::TypeOfData>& available_types
):
	  name(name),
	  available_types(available_types) {}

const base::HashMap<base::StrID, vm::code::TypeOfData>&
	vm::code::builders::CodeFileBuilder::getAvailableTypes() const {
	return type_map;
}

usize getTypeSize(const vm::code::TypeOfData& tp) {
	variant_match(tp) {
		variant_case(vm::code::PrimitiveType, primitive) return primitive.size;
		NOIMPL_CASE(vm::code::PointerType, "get size of")
		NOIMPL_CASE(vm::code::StaticTableType, "get size of")
		NOIMPL_CASE(vm::code::DynamicTableType, "get size of")
		NOIMPL_CASE(vm::code::DataType, "get size of")
		NOIMPL_CASE(vm::code::VariantType, "get size of")
		NOIMPL_CASE(vm::code::FunctionType, "get size of")
	}
	CORE_UNREACHABLE();
}

usize vm::code::builders::FunctionBuilder::initType(base::StrID tp) {
	instructions.emplace_back(instructions::Op_init_type{ tp });

	const vm::code::TypeOfData& vm_type   = available_types[tp];
	const usize                 type_size = getTypeSize(vm_type);
	usize                       offset    = 0;

	if (!local_stack.empty()) {
		const LocalStackEntry& prev_entry = local_stack.back();
		offset                            = prev_entry.local_stack_position + prev_entry.type_size;
	}

	local_stack.emplace_back(LocalStackEntry{
		.tp = tp, .local_stack_position = offset, .type_size = type_size });

	max_stack_size = std::max(max_stack_size, offset + type_size);

	return offset;
}

void vm::code::builders::FunctionBuilder::deinitType() {
	CORE_ASSERT(!local_stack.empty(), "Popping from empty variable stack");
	instructions.emplace_back(instructions::Op_deinit{});
	local_stack.pop_back();
}

usize vm::code::builders::FunctionBuilder::getLocalSize() const { return local_stack.size(); }

void vm::code::builders::InstructionBuilder::pushArg(const vm::opargs::OpCodeArg& arg) {
	args.push_back(arg);
}

void vm::code::builders::FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

vm::code::builders::InstructionBuilder::InstructionBuilder(OpKind kind) { setKind(kind); }
