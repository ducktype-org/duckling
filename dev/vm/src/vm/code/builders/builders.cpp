#include "builders.hpp"
#include "base/optional.hpp"
#include "vm/code/instructions.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
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

using namespace vm::code::builders;

vm::code::Function FunctionBuilder::build() const {
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

void CodeFileBuilder<AddingTypes>::addType(const vm::code::TypeOfData& type) {
	auto name = VISIT(type, tp, return tp.name);
	match_optional(cfb.types_of_data.atMaybe(name)) {
		opt_some(tp) { CORE_ASSERT(tp == type, "Duplicated type name: " + name.str()); }
		opt_none {
			cfb.types.addType(Type::declareType(name));
			cfb.types_of_data.put(name, type);
		}
	}
	CORE_UNREACHABLE();
}

vm::code::CodeFile CodeFileBuilder::build(bool dump_types) const {
	auto file = CodeFile();
	if (dump_types)
		for (const auto& tod: types.TYPES_OF_DATA | std::views::values) file.types.push_back(tod);
	file.functions = std::ranges::to<std::vector<Function>>(
		functions | std::views::transform([&](const auto& function_builder) {
			return function_builder.build();
		})
	);
	return file;
}

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	variant_match(instruction) {
		variant_case(instructions::Op_init_type, instr) {
			initType(instr);
			return;
		}
		variant_case(instructions::Op_deinit, instr) {
			deinitType();
			return;
		}
		variant_case(instructions::Op_label, label) {
			handleLabel(label);
			return;
		}
		variant_case(instructions::Op_jmpRel_label, jmp) { saveStackState(jmp.arg0.label_name); }
		variant_case(instructions::Op_jmpRelIf_label, jmp) { saveStackState(jmp.arg0.label_name); }
		variant_case(instructions::Op_jmpRelNotIf_label, jmp) {
			saveStackState(jmp.arg0.label_name);
		}
	}
	instructions.push_back(instruction);
}

FunctionBuilder::FunctionBuilder(base::StrID name, const TypeMetadata& types):
	  name(name),
	  types(types) {}

usize FunctionBuilder::initType(instructions::Op_init_type init) {
	instructions.emplace_back(init);

	const usize type_size = types.at(init.arg0.type_name)->getSize();
	usize       offset    = 0;

	if (!local_stack.empty()) {
		const LocalStackEntry& prev_entry = local_stack.back();
		offset                            = prev_entry.local_stack_position + prev_entry.type_size;
	}

	local_stack.emplace_back(LocalStackEntry{ .unique_id            = LocalStackEntryID::next(),
	                                          .tp                   = init.arg0.type_name,
	                                          .local_stack_position = offset,
	                                          .type_size            = type_size });

	max_stack_size = std::max(max_stack_size, offset + type_size);

	return offset;
}

void FunctionBuilder::deinitType() {
	CORE_ASSERT(!local_stack.empty(), "Popping from empty variable stack");
	instructions.emplace_back(instructions::Op_deinit{});
	local_stack.pop_back();
}

usize FunctionBuilder::getLocalSize() const { return local_stack.size(); }

void InstructionBuilder::pushArg(const vm::opargs::OpCodeArg& arg) { args.push_back(arg); }

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

InstructionBuilder::InstructionBuilder(OpKind kind) { setKind(kind); }

void FunctionBuilder::handleLabel(instructions::Op_label label) {
	if (!stack_state_at_label.contains(label.arg0.label_name))
		stack_state_at_label.put(label.arg0.label_name, local_stack);
	else
		local_stack = stack_state_at_label[label.arg0.label_name];
	instructions.emplace_back(label);
}

void FunctionBuilder::saveStackState(base::StrID at_label_name) {
	if (stack_state_at_label.contains(at_label_name)) {
		if (stack_state_at_label[at_label_name] == local_stack)
			throw builders::BuilderError("Stack state differs");
	} else {
		stack_state_at_label.put(at_label_name, local_stack);
	}
}

CodeFileBuilder<FinalizedTypes> CodeFileBuilder<AddingTypes>::finalize() {
	for (const auto& type: cfb.types_of_data) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				cfb.types.at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				cfb.types.at(data.name)->definePointer(cfb.types.at(data.inner));
			}
			variant_case(vm::code::StaticTableType, data) {
				cfb.types.at(data.name)->defineStaticTable(
					cfb.types.at(data.inner), data.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, data) {
				cfb.types.at(data.name)->defineDynamicTable(cfb.types.at(data.inner));
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(field.name, cfb.types.at(field.type));
				cfb.types.at(data.name)->defineData(fields);
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(cfb.types.at(variant));
				cfb.types.at(data.name)->defineVariant(variants);
			}
			variant_case(vm::code::FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters) parameters.emplace_back(cfb.types.at(param));
				cfb.types.at(data.name)->defineFunction(parameters, cfb.types.at(data.result));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	cfb.types.finalize();
	return cfb;
}

vm::code::builders::CodeFileBuilder::CodeFileBuilder(
	TypesContext<types_context_state::Finalized> types
):
	  types(std::move(types)) {}
