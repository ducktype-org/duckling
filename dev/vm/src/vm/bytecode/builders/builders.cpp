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

vm::code::Function FunctionBuilder::build() {
	validate();

	Function function;
	function.body = instructions;
	function.name = name;
	return function;
}

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

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	instructions.push_back(instruction);
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

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

void FunctionBuilder::validateInstruction(const Instruction& instruction) const {
	// @TODO check args for non-control flow instruction etc.
}

void FunctionBuilder::pushStackState(opargs::StackLocalAny local, opargs::Type type) {
	auto tod = type_context.getTypes().atMaybe(type.type_name).expect<UnknownTypeError>(type);

	if (local_name_to_type.contains(local.var_name)) throw DuplicateLocalNameError(local);

	stack_state.emplace_back(local.var_name, tod);
	local_name_to_type.put(local.var_name, tod);
}

// @TODOB this should probably put the instruction responsible for the error in the builder error
void FunctionBuilder::popStackState() {
	if (stack_state.empty()) throw EmptyStackDeinitError();
	const auto& top = stack_state.back();
	local_name_to_type.erase(top.local_name);
	stack_state.pop_back();
}

void FunctionBuilder::popCallArgs(opargs::FunctionName function) {
	// @TODOB is this the right error?
	auto maybe_func_type = type_context.getTypes()
	                           .atMaybe(function.function_name)
	                           .expect<MissingFunctionalTypeError>(function.function_name);
	if (!std::holds_alternative<FunctionType>(*maybe_func_type))
		throw TypeIsNotFunctionalError(name);
	auto func_type = std::get<FunctionType>(*maybe_func_type);

	if (func_type.parameters.size() > stack_state.size()) throw InvalidFunctionCallArguments();
	for (auto param: func_type.parameters | std::views::reverse) {
		if (code::typeName(*stack_state.back().type) != param) throw InvalidFunctionCallArguments();
		stack_state.pop_back();
	}
}

void FunctionBuilder::validateReturnValue() const {
	if (stack_state.empty()) throw BadReturnError();
	const auto& bottom = stack_state[0];
	// @TODOB czy tu ifować voida?
	if (bottom.local_name != "ret_val" || code::typeName(*bottom.type) != type.result)
		throw BadReturnError();
}

FunctionBuilder::InstructionIter FunctionBuilder::getLabelTarget(opargs::Label label) const {
	return instruction_at_label.atMaybe(label.label_name).expect<UnknownLabelError>(label);
}

void FunctionBuilder::validate() {
	pushStackState(base::StrID("ret_val"), type.result);
	for (auto [idx, param]: std::views::enumerate(type.parameters))
		pushStackState(base::StrID(base::strConcat("arg", idx).c_str()), param);

	for (auto iter = instructions.cbegin(); iter != instructions.cend(); ++iter) {
		variant_match(*iter) {
			variant_case(Op_label, instr) {
				auto [_, added]
					= instruction_at_label.insert_or_assign(instr.arg0.label_name, iter);
				if (!added) throw DuplicateLabelError(instr.arg0);
			}
		}
	}

	std::vector<std::pair<InstructionIter, decltype(stack_state)>> dfs_stack{
		{ instructions.cend(), {} }  // sentinel
	};
	auto iter = instructions.cbegin();

	while (iter != instructions.cend()) {
		// Validate non-control flow instruction there.
		validateInstruction(*iter);

		variant_match(*iter) {
			variant_case(Op_init_lany_type, instr) {
				pushStackState(instr.arg0, instr.arg1);
				++iter;
			}
			variant_case(Op_deinit, instr) {
				popStackState();
				++iter;
			}
			variant_case(Op_label, instr) {
				match_optional(stack_at_label.atMaybe(instr.arg0.label_name)) {
					opt_some(label_state) {
						if (label_state != stack_state) throw StackStructureMismatchError({});
						std::tie(iter, stack_state) = dfs_stack.back();
                        dfs_stack.pop_back();
					}
					opt_none {
						stack_at_label.put(instr.arg0.label_name, stack_state);
						++iter;
					}
				}
			}
			variant_case(Op_jmp_label, instr) { iter = getLabelTarget(instr.arg0); }
			variant_case(Op_jmpIf_label, instr) {
				++iter;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), stack_state);
			}
			variant_case(Op_jmpIfNot_label, instr) {
				++iter;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), stack_state);
			}
			variant_case(Op_ret, instr) {
				validateReturnValue();
				std::tie(iter, stack_state) = dfs_stack.back();
				dfs_stack.pop_back();
			}
			variant_case(Op_call_func, instr) {
				popCallArgs(instr.arg0);
				++iter;
			}
			variant_case(Op_call_builtin_func, instr) {
				// @TODOB really?
				opargs::FunctionName hack(instr.arg0.function_name);
				hack.bytecode_pos = instr.arg0.bytecode_pos;
				popCallArgs(hack);
				++iter;
			}
			variant_case(Op_ret_tailcall_func, instr) {
				popCallArgs(instr.arg0);
				std::tie(iter, stack_state) = dfs_stack.back();
				dfs_stack.pop_back();
			}
            variant_default {
                ++iter;
            }
		}
	}
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
			if (type != *tp) throw DuplicatedTypeError(name);
		}
		opt_none { types.insert(type, name); }
	}
}
