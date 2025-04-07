#include "builders.hpp"

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
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
	function.arg_size         = type_context.getMetadata().at(name)->getParametersSize().value();
	function.next_arg_size    = 0;

	return function;
}

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	using namespace instructions;
	variant_match(instruction) {
		variant_case(Op_init_type, instr) { pushStackState(instr.arg0); }
		variant_case(Op_deinit, instr) { handleDeinit(); }
		variant_case(Op_label, label) { handleLabel(label); }
		variant_case(Op_call_func, func) { handleCall(func.arg0.function_name); }
		variant_case(Op_jmpRel_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(Op_jmpRelIf_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(Op_jmpRelNotIf_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(Op_ret, ret) { handleRet(); }
		variant_case(Op_ret_tailcall_func, tailcall) {
			verifyCall(tailcall.arg0);
			handleRet();
		}
	}
	instructions.push_back(instruction);
}

FunctionBuilder::FunctionBuilder(base::StrID name, const TypeContext& types):
	  name(name),
	  type_context(types) {
	TypeCRef func_result_type = type_context.getMetadata()
	                                .atMaybe(name)
	                                .expect<MissingFunctionalTypeError>(name)
	                                ->getResultType()
	                                .expect<TypeIsNotFunctionalError>(name);
	initType(instructions::Op_init_type{ func_result_type->getName() });
	instructions.pop_back();
	auto params = type_context.getMetadata().at(name)->getParameters().value();
	for (TypeCRef param: params) pushStackState({ param->getName() });
}

usize vm::code::builders::FunctionBuilder::pushStackState(vm::opargs::Type type) {
	const usize type_size = type_context.getMetadata()
	                            .atMaybe(type.type_name)
	                            .expect<UnknownTypeError>(type)
	                            ->getSize();

	usize offset = 0;

	if (!local_stack.empty()) {
		const LocalStackEntry& prev_entry = local_stack.back();
		offset                            = prev_entry.local_stack_position + prev_entry.type_size;
	}

	local_stack.emplace_back(LocalStackEntry{ .unique_id            = LocalStackEntryID::next(),
	                                          .type_name            = type.type_name,
	                                          .local_stack_position = offset,
	                                          .type_size            = type_size });

	max_stack_size = std::max(max_stack_size, offset + type_size);

	return offset;
}

usize FunctionBuilder::initType(instructions::Op_init_type init) {
	instructions.emplace_back(init);
	return pushStackState(init.arg0);
}

void FunctionBuilder::handleDeinit() {
	if (local_stack.empty()) throw EmptyStackDeinitError();
	local_stack.pop_back();
}

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

void FunctionBuilder::handleLabel(instructions::Op_label label) {
	saveStackState(label.arg0.label_name);
	label_users[label.arg0.label_name].emplace_back(label);
}

void vm::code::builders::FunctionBuilder::verifyCall(opargs::FunctionName function) {
	auto func_type = type_context.getMetadata()
	                     .atMaybe(function.function_name)
	                     .expect<MissingFunctionalTypeError>(function.function_name);
	auto param_count
		= func_type->getParameterCount().expect<TypeIsNotFunctionalError>(function.function_name);

	auto min_stack_size = param_count + 1;  // +1 because return value
	if (func_type->getResultType().value()->getSize() == 0) {
		// return value is void
		if (local_stack.size() < min_stack_size - 1) throw InvalidFunctionCallArguments();
	} else {
		// Too few arguments
		if (local_stack.size() < min_stack_size) throw InvalidFunctionCallArguments();
		// Invalid result type
		if (local_stack.at(local_stack.size() - 1 - param_count).type_name
		    != func_type->getResultType().value()->getName()) {
			throw InvalidFunctionCallArguments();
		}
	}

	for (usize i = 0; i < param_count; i++) {
		// Invalid arguments
		auto tp = func_type->getNthParameterType(i).value();
		if (tp->getName() != local_stack.at(local_stack.size() - 1 - i).type_name)
			throw InvalidFunctionCallArguments();
	}
}

void FunctionBuilder::handleCall(vm::opargs::FunctionName function) {
	verifyCall(function);
	auto param_count = type_context.getMetadata()
	                       .atMaybe(function.function_name)
	                       .expect<MissingFunctionalTypeError>(function.function_name)
	                       ->getParameterCount()
	                       .expect<TypeIsNotFunctionalError>(function.function_name);
	for (usize i = 0; i < param_count; i++) local_stack.pop_back();
}

void FunctionBuilder::saveStackState(vm::opargs::Label at_label) {
	if (stack_state_at_label.contains(at_label.label_name)) {
		if (stack_state_at_label[at_label.label_name] != local_stack)
			throw builders::StackStructureMismatchError(label_users.at(at_label.label_name));
	} else {
		stack_state_at_label.put(at_label.label_name, local_stack);
		label_users.put(at_label.label_name, {});
	}
}

void vm::code::builders::FunctionBuilder::handleRet() {
	if (local_stack.empty()
	    || local_stack.at(0).type_name
	           != type_context.getMetadata().at(name)->getResultType().value()->getName()) {
		throw BadReturnError();
	}
}

void vm::code::builders::FunctionBuilder::setRetSize(usize ret_size) { this->ret_size = ret_size; }

const vm::StableTypeIdNameMap<vm::code::TypeOfData>&
	vm::code::builders::TypeContextBuilder::getTypes() const {
	return types;
}

TypeContext TypeContextBuilder::build() const {
	TypeContext tctx;
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
				std::vector<std::pair<base::StrID, TypeRef>> fields;
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

const std::vector<vm::code::TypeOfData>& TypeContext::getTypes() const { return types; }

const vm::TypeMetadata& TypeContext::getMetadata() const { return *metadata; }

Box<vm::TypeMetadata> TypeContext::moveMetadata() && { return std::move(metadata); }

void TypeContextBuilder::addType(const TypeOfData& type) {
	const auto name = VISIT(type, tp, return tp.name);
	match_optional(types.atMaybe(name)) {
		opt_some(tp) {
			if (type != *tp) throw DuplicatedTypeError(name);
		}
		opt_none { types.insert(type, name); }
	}
}
