#include "builders.hpp"

#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <ranges>

using namespace vm::code::builders;

namespace {
	// Helpers for validation `ext_*` instructions
	using namespace vm::code;
	using namespace vm::code::instructions;

	template<typename T, typename Tup>
	struct IsIn;

	template<typename T, typename... Ts>
	struct IsIn<T, std::tuple<Ts...>> {
		static constexpr bool VALUE = (std::same_as<T, Ts> || ...);
	};

	template<typename... Tups>
	using Cat = decltype(std::tuple_cat(std::declval<Tups>()...));

	template<typename Tup>
	struct HoldsOneOfImpl;

	template<typename... Ts>
	struct HoldsOneOfImpl<std::tuple<Ts...>> {
		constexpr bool operator()(const Instruction& instr) {
			return (std::holds_alternative<Ts>(instr) || ...);
		}
	};

	template<typename Tup>
	constexpr bool holdsOneOf(const Instruction& instr) {
		return HoldsOneOfImpl<Tup>{}(instr);
	}

	using ExtensionTypes = std::tuple<Op_ext_l64, Op_ext_type>;
	template<typename T>
	concept Extension = IsIn<T, ExtensionTypes>::VALUE;

	template<Extension E>
	struct ExtensionMetadata;

	template<>
	struct ExtensionMetadata<Op_ext_l64> {
		using RequiredAfter = std::tuple<>;
		using OptionalAfter = std::tuple<Op_load_l64_lptr_ofs, Op_store_lptr_l64_ofs>;
	};

	template<>
	struct ExtensionMetadata<Op_ext_type> {
		using RequiredAfter = std::tuple<Op_downcast_lptr_lptr>;
		using OptionalAfter = std::tuple<>;
	};

	template<typename Tup>
	struct CatRequired;

	template<typename... Ts>
	struct CatRequired<std::tuple<Ts...>> {
		using Value = Cat<typename ExtensionMetadata<Ts>::RequiredAfter...>;
	};

	template<Extension E>
	bool acceptsExtension(const Instruction& instr) {
		return holdsOneOf<
			Cat<typename ExtensionMetadata<E>::RequiredAfter,
		        typename ExtensionMetadata<E>::OptionalAfter>>(instr);
	}

	bool requiresSomeExtension(const Instruction& instr) {
		return holdsOneOf<CatRequired<ExtensionTypes>::Value>(instr);
	}
}

void FunctionBuilder::validateExtension(usize index) const {
	// This check assumes that the last instruction in a function is non-extendable,
	// this is the case for `ret`.
	auto instruction = instructions[index];
	auto predecessor = index == 0 ? base::Optional<const Instruction&>{} : instructions[index - 1];
	bool valid_extension = std::visit(
		[&]<typename T>(const T&) {
			if constexpr (Extension<T>)
				// If the current instruction is an extension, the situation is valid
			    // if the previous instruction can take this extension.
			    // Extensions must always come after some instruction, so it's invalid for it to be
			    // the first instruction in a function (to not have a predecessor).
				return predecessor.map(acceptsExtension<T>).valueOr(false);
			else
				// It is invalid if the current instruction is not an extension, but the previous
			    // instruction *requires* one. If there was no previous instruction, it's not invalid.
				return !predecessor.map(requiresSomeExtension).valueOr(false);
		},
		instruction
	);
	if (!valid_extension) throw InvalidInstructionExtensionError(instruction);
}

void FunctionBuilder::validateInstruction(const Instruction& instruction) const {
	variant_match(instruction) {
		variant_case(Op_init_lany_type, instr) { validateArgInstantiable(instr.arg1); }
		variant_case(Op_alloc_lptr_type, instr) { validateArgInstantiable(instr.arg1); }
	}
}

void FunctionBuilder::validateArgInstantiable(const opargs::Type& arg) const {
	// @TODO remove `atMaybe` after #732
	auto type = type_context.getMetadata().atMaybe(arg.type_name).expect<UnknownTypeError>(arg);
	if (!type->isInstantiable()) throw UninstantiableValueError(arg);
}

FunctionBuilder::FunctionBuilder(base::StrID name, const TypeContext& types):
	  name(name),
	  type_context(types),
	  type([&] {
		  auto maybe_func_type
			  = type_context.getTypes().atMaybe(name).expect<MissingFunctionalTypeError>(name);
		  if (!std::holds_alternative<FunctionType>(*maybe_func_type))
			  throw TypeIsNotFunctionalError(name);
		  return std::get<FunctionType>(*maybe_func_type);
	  }()) {}

vm::code::Function FunctionBuilder::build() {
	processControlFlowGraph();

	Function function;
	function.body = instructions;
	function.name = name;
	return function;
}

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	instructions.push_back(instruction);
}

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

void FunctionBuilder::pushStackState(opargs::StackLocalAny local, opargs::Type type) {
	auto tod = type_context.getTypes().atMaybe(type.type_name).expect<UnknownTypeError>(type);

	if (local_name_to_type.contains(local.var_name)) throw DuplicateLocalNameError(local);

	stack_state.emplace_back(local.var_name, tod);
	local_name_to_type.put(local.var_name, tod);
}

void FunctionBuilder::popStackState(const Op_deinit& cause) {
	if (stack_state.size() == 1) throw RetValDeinitError(cause);
	const auto& top = stack_state.back();
	local_name_to_type.erase(top.local_name);
	stack_state.pop_back();
}

void FunctionBuilder::popCallArgs(opargs::OpCodeFunctionArg function, bool check_ret_val) {
	auto fun_name = VISIT(function, f, return f.function_name);
	// Used for errors.
	auto generic_arg = VISIT(function, f, return opargs::OpCodeArg{ f });
	auto maybe_func_type
		= type_context.getTypes().atMaybe(fun_name).expect<UnknownFunctionError>(generic_arg);
	if (!std::holds_alternative<FunctionType>(*maybe_func_type))
		throw UnknownFunctionError(generic_arg);
	auto func_type = std::get<FunctionType>(*maybe_func_type);
	check_ret_val  = check_ret_val && func_type.result != base::StrID("void");

	if (func_type.parameters.size() > stack_state.size() + check_ret_val)
		throw InvalidFunctionCallArgumentsError(generic_arg);
	for (auto param: func_type.parameters | std::views::reverse) {
		if (code::typeName(*stack_state.back().type) != param)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		stack_state.pop_back();
	}
	if (check_ret_val && code::typeName(*stack_state.back().type) != func_type.result)
		throw InvalidFunctionCallArgumentsError(generic_arg);
}

usize FunctionBuilder::getLabelTarget(opargs::Label label) const {
	return index_of_label.atMaybe(label.label_name).expect<UnknownLabelError>(label);
}

void FunctionBuilder::preprocessLabels() {
	auto register_jump = [&](auto instr) {
		jumps_to_label.try_emplace(instr.arg0.label_name);
		jumps_to_label.at(instr.arg0.label_name).push_back(instr);
	};

	for (usize index = 0; index < instructions.size(); index++) {
		variant_match(instructions[index]) {
			variant_case(Op_label, instr) {
				auto [_, added] = index_of_label.insert_or_assign(instr.arg0.label_name, index);
				if (!added) throw DuplicateLabelError(instr.arg0);
			}
			variant_case(Op_jmp_label, instr) { register_jump(instr); }
			variant_case(Op_jmpIf_label, instr) { register_jump(instr); }
			variant_case(Op_jmpIfNot_label, instr) { register_jump(instr); }
		}
	}
}

void FunctionBuilder::processControlFlowGraph() {
	preprocessLabels();

	pushStackState(base::StrID("ret_val"), type.result);
	for (auto [idx, param]: std::views::enumerate(type.parameters))
		pushStackState(base::StrID(base::strConcat("arg", idx).c_str()), param);

	std::vector<bool>                                    visited_instructions(instructions.size());
	std::vector<std::pair<usize, decltype(stack_state)>> dfs_stack{
		{ instructions.size(), {} }  // sentinel
	};
	usize index = 0;

	while (index != instructions.size()) {
		// Validate non-control flow instruction there.
		validateExtension(index);
		validateInstruction(instructions[index]);
		visited_instructions[index] = true;

		variant_match(instructions[index]) {
			variant_case(Op_init_lany_type, instr) {
				pushStackState(instr.arg0, instr.arg1);
				index++;
			}
			variant_case(Op_deinit, instr) {
				popStackState(instr);
				index++;
			}
			variant_case(Op_label, instr) {
				match_optional(stack_at_label.atMaybe(instr.arg0.label_name)) {
					opt_some(label_state) {
						if (label_state != stack_state)
							throw StackStructureMismatchError(
								instr, jumps_to_label.at(instr.arg0.label_name)
							);
						std::tie(index, stack_state) = dfs_stack.back();
						dfs_stack.pop_back();
					}
					opt_none {
						stack_at_label.put(instr.arg0.label_name, stack_state);
						index++;
					}
				}
			}
			variant_case(Op_jmp_label, instr) { index = getLabelTarget(instr.arg0); }
			variant_case(Op_jmpIf_label, instr) {
				index++;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), stack_state);
			}
			variant_case(Op_jmpIfNot_label, instr) {
				index++;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), stack_state);
			}
			variant_case(Op_ret, instr) {
				std::tie(index, stack_state) = dfs_stack.back();
				dfs_stack.pop_back();
			}
			variant_case(Op_call_func, instr) {
				popCallArgs(instr.arg0);
				index++;
			}
			variant_case(Op_call_builtin_func, instr) {
				popCallArgs(instr.arg0);
				index++;
			}
			variant_case(Op_ret_tailcall_func, instr) {
				popCallArgs(instr.arg0, false);
				std::tie(index, stack_state) = dfs_stack.back();
				dfs_stack.pop_back();
			}
			variant_default { index++; }
		}
	}

	// Since dead code does not get checked, elimate it.
	for (auto [visited, instruction]: std::views::zip(visited_instructions, instructions))
		if (!visited) instruction = Comment(base::StrID("DEAD CODE"));
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& vm::code::builders::TypeContextBuilder::getTypes(
) const {
	return types;
}

void TypeContextBuilder::validateType(const TypeOfData& type) const {
	auto get_type = [&](base::StrID name) {
		return types.atMaybe(name).expect<UnknownSubtypeError>(type, name);
	};
	auto validate_implements = [&](const std::vector<base::StrID>& implements) {
		for (const auto& impl: implements) {
			auto impl_type = get_type(impl);
			if (!std::holds_alternative<InterfaceType>(*impl_type) || *impl_type == type)
				throw InvalidImplementsError(type);
		}
	};

	variant_match(type) {
		variant_case(InterfaceType, interface) { validate_implements(interface.implements); }
		variant_case(ClassType, clazz) {
			validate_implements(clazz.implements);
			if_opt_some(clazz.extends, extends) {
				auto super_type = get_type(extends);
				if (!std::holds_alternative<ClassType>(*super_type) || *super_type == type)
					throw InvalidExtends(type);

				const auto& superclass = std::get<ClassType>(*super_type);
				if (clazz.fields.size() < superclass.fields.size())
					throw MissingAncestorFieldError(type);
				for (auto [field, super_field]: std::views::zip(clazz.fields, superclass.fields))
					if (field != super_field) throw MissingAncestorFieldError(type);
			}
		}
	}
}

void TypeContextBuilder::validateTypes() const {
	for (const auto& type: types) validateType(type);

	// Check for cycles in hierarchy.
	enum Status { Waiting, Visited, Done };

	base::HashMap<base::StrID, Status> status;
	for (const auto& type: types) status.put(typeName(type), Waiting);

	// explicit object parameter lambdas don't seem to work with class members, hence the reference
	auto& types_ref = types;
	auto  helper    = [&](this auto self, const auto& type) {
        auto name = typeName(type);
        if (status[name] == Visited)
            throw CycleInHierarchyError(type);
        else if (status[name] == Done)
            return;

        status[name] = Visited;
        variant_match(type) {
            variant_case(ClassType, clazz) {
                if_opt_some(clazz.extends, superclass) self(*types_ref.at(superclass));
                for (auto iface: clazz.implements) self(*types_ref.at(iface));
            }
            variant_case(InterfaceType, interface) {
                for (auto iface: interface.implements) self(*types_ref.at(iface));
            }
        }
        status[name] = Done;
	};

	for (const auto& type: types) helper(type);
}

TypeContext TypeContextBuilder::build() const {
	validateTypes();
	TypeContext tctx;
	tctx.types = types;
	for (const auto& type: types)
		tctx.metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));
	auto to_low_type
		= [&](const TypeOfData& tod) { return tctx.metadata->at(VISIT(tod, tp, return tp.name)); };
	for (const auto& type: tctx.types) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				tctx.metadata->at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				tctx.metadata->at(data.name)->definePointer(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::StaticTableType, data) {
				tctx.metadata->at(data.name)->defineStaticTable(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner),
					data.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, data) {
				tctx.metadata->at(data.name)->defineDynamicTable(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						tctx.metadata->atMaybe(field.type)
							.expect<UnknownSubtypeError>(data, field.name)
					);
				tctx.metadata->at(data.name)->defineData(fields, {});
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(
						tctx.metadata->atMaybe(variant).expect<UnknownSubtypeError>(data, variant)
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
					tctx.metadata->atMaybe(data.result).expect<UnknownSubtypeError>(data, data.result)
				);
			}
			variant_case(vm::code::OpaqueType, opaque) {
				tctx.metadata->at(opaque.name)->defineOpaque(opaque.size);
			}
			variant_case(vm::code::ClassType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				fields.reserve(data.fields.size() + 1);
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						tctx.metadata->atMaybe(field.type)
							.expect<UnknownSubtypeError>(data, field.name)
					);
				TypeRef tp       = tctx.metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};
				auto kind = InheritanceMetadata::Class{
					.is_abstract = data.is_abstract,
					.extends     = data.extends.map(get_type),
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();
				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type(method.type));
				vm::InheritanceMetadata inheritance_metadata(
					tp, kind, std::move(implements), std::move(virtual_methods)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_case(vm::code::InterfaceType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				TypeRef tp       = tctx.metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();
				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type(method.type));
				vm::InheritanceMetadata inheritance_metadata(
					tp,
					InheritanceMetadata::Interface{},
					std::move(implements),
					std::move(virtual_methods)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	tctx.metadata->finalize();
	return tctx;
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& TypeContext::getTypes() const { return types; }

const vm::TypeMetadata& TypeContext::getMetadata() const { return *metadata; }

Box<vm::TypeMetadata> TypeContext::moveMetadata() && { return std::move(metadata); }

void TypeContextBuilder::addType(const TypeOfData& type) {
	const auto name = VISIT(type, tp, return tp.name);
	match_optional(types.atMaybe(name)) {
		opt_some(tp) {
			if (type != *tp) throw DuplicateTypeError(type);
		}
		opt_none { types.insert(type, name); }
	}
}
