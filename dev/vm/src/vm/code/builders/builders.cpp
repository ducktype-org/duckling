#include "builders.hpp"

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <vm/code/builders/errors.hpp>
#include <vm/code/instructions.hpp>
#include <vm/code/opcode_args.hpp>
#include <vm/code/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

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

	// @TODO: ret_size will be fixed in this issue:
	// https://github.com/ducktype-org/duckling/issues/539
	function.local_stack_size = max_stack_size;
	function.ret_size         = ret_size;
	function.arg_size         = types_context.getMetadata().at(name)->getParametersSize().value();
	function.next_arg_size    = 0;

	return function;
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
		variant_case(instructions::Op_call_func, func) {
			handleCallFunc(func);
			return;
		}
		variant_case(instructions::Op_jmpRel_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(instructions::Op_jmpRelIf_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(instructions::Op_jmpRelNotIf_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
	}
	instructions.push_back(instruction);
}

FunctionBuilder::FunctionBuilder(
	base::StrID name, const TypesContext<TypesContextState::Finalized>& types
):
	  name(name),
	  types_context(types) {
	TypeCRef func_result_type = types_context.getMetadata()
	                                .atMaybe(name)
	                                .expect<MissingFunctionalTypeError>(name)
	                                ->getResultType()
	                                .expect<TypeIsNotFunctionalError>(name);
	initType(instructions::Op_init_type{ func_result_type->getName() });
	instructions.pop_back();
	auto params = types_context.getMetadata().at(name)->getParameters().value();
	for (TypeCRef param: params) {
		initType(instructions::Op_init_type{ param->getName() });
		instructions.pop_back();
	}
}

usize FunctionBuilder::initType(instructions::Op_init_type init) {
	instructions.emplace_back(init);

	const usize type_size = types_context.getMetadata()
	                            .atMaybe(init.arg0.type_name)
	                            .expect<UnknownTypeError>(init.arg0)
	                            ->getSize();

	usize offset = 0;

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

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

void FunctionBuilder::handleLabel(instructions::Op_label label) {
	saveStackState(label.arg0.label_name);
	instructions.emplace_back(label);
	label_users[label.arg0.label_name].emplace_back(label);
}

void FunctionBuilder::handleCallFunc(instructions::Op_call_func func) {
	auto func_name = func.arg0.function_name;
	auto params    = types_context.getMetadata()
	                  .atMaybe(func_name)
	                  .expect<MissingFunctionalTypeError>(func_name)
	                  ->getParameterCount()
	                  .expect<TypeIsNotFunctionalError>(func_name);
	for (usize i = 0; i < params; i++) local_stack.pop_back();
	instructions.emplace_back(func);
}

void FunctionBuilder::saveStackState(base::StrID at_label_name) {
	if (stack_state_at_label.contains(at_label_name)) {
		if (stack_state_at_label[at_label_name] != local_stack)
			throw builders::StackStructureMismatchError(label_users.at(at_label_name));
	} else {
		stack_state_at_label.put(at_label_name, local_stack);
		label_users.put(at_label_name, {});
	}
}

void vm::code::builders::FunctionBuilder::setRetSize(usize ret_size) { this->ret_size = ret_size; }

const base::StableTypeIdNameMap<vm::code::TypeOfData, usize>&
	vm::code::builders::TypesContext<TypesContextState::AddingTypes>::getTypes() const {
	return types;
}

TypesContext<TypesContextState::Finalized>
	TypesContext<TypesContextState::AddingTypes>::finalized() const {
	TypesContext<TypesContextState::Finalized> tctx;
	for (const auto& type: types) {
		tctx.metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));
		tctx.types.push_back(type);
	}
	for (const auto& type: tctx.types) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				tctx.metadata->at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				tctx.metadata->at(data.name)->definePointer(
					tctx.metadata->atMaybe(data.inner).expect<MissingSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::StaticTableType, data) {
				tctx.metadata->at(data.name)->defineStaticTable(
					tctx.metadata->atMaybe(data.inner)
						.expect<MissingSubtypeError>(data, data.inner),
					data.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, data) {
				tctx.metadata->at(data.name)->defineDynamicTable(
					tctx.metadata->atMaybe(data.inner).expect<MissingSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						tctx.metadata->atMaybe(field.type)
							.expect<MissingSubtypeError>(data, field.name)
					);
				tctx.metadata->at(data.name)->defineData(fields);
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(
						tctx.metadata->atMaybe(variant).expect<MissingSubtypeError>(data, variant)
					);
				tctx.metadata->at(data.name)->defineVariant(variants);
			}
			variant_case(vm::code::FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters)
					parameters.emplace_back(tctx.metadata->at(param));
				tctx.metadata->at(data.name)->defineFunction(
					parameters,
					tctx.metadata->atMaybe(data.result)
						.expect<MissingSubtypeError>(data, data.result)
				);
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	tctx.metadata->finalize();
	return tctx;
}

const std::vector<vm::code::TypeOfData>&
	TypesContext<TypesContextState::Finalized>::getTypes() const {
	return types;
}

const vm::TypeMetadata& TypesContext<TypesContextState::Finalized>::getMetadata() const {
	return *metadata;
}

Box<vm::TypeMetadata> TypesContext<TypesContextState::Finalized>::moveMetadata() && {
	return std::move(metadata);
}

void TypesContext<>::addType(const TypeOfData& type) {
	const auto name = VISIT(type, tp, return tp.name);
	match_optional(types.atMaybe(name)) {
		opt_some(tp) {
			if (type != *tp) throw DuplicatedTypeError(name);
		}
		opt_none { types.insert(type, name); }
	}
}
