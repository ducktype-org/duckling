#include "builders.hpp"

#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <algorithm>
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
	// @TODO this should be the compiler's responsibility, move it there
	function.local_offset_map = local_offset_map;
	function.local_stack_size = max_stack_size;

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

void FunctionBuilder::validateArgInstantiable(const opargs::Type& arg) {
	// @TODO remove `atMaybe` after #732
	auto type = type_context.getMetadata().atMaybe(arg.type_name).expect<UnknownTypeError>(arg);
	if (!type->isInstantiable()) throw UninstantiableValueError();
}

void FunctionBuilder::validateExtension(const Instruction& instruction) {
	// This check assumes that the last instruction in a function is non-extendable,
	// this is the case for `ret`.
	auto predecessor
		= instructions.empty() ? base::Optional<const Instruction&>{} : instructions.back();
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
	if (!valid_extension) throw InvalidInstructionExtensionError();
}

void FunctionBuilder::validateLocalArgs(const Instruction& instruction) const {
	// @TODO this check is temporary (and therefore kinda unpolished),
	// will get done redone in graph jumps.

	// Wrap the arguments in a variant in order to ease manipulation.
	auto args = std::visit(
		[]<typename T>(T& instr) -> std::vector<opargs::OpCodeArg> {
			if constexpr (TwoArgumentOpcode<T>)
				return { instr.arg0, instr.arg1 };
			else if constexpr (OneArgumentOpcode<T>)
				return { instr.arg0 };
			else
				return {};
		},
		instruction
	);

	// The check whether a variable is initialised exactly once
	// happens in `pushStackState` (to make sure manual variable definition is checked as well),
	// so we skip the `init` instructions here.
	if (!std::holds_alternative<instructions::Op_init_lany_type>(instruction)) {
		for (auto arg: args) {
			variant_match(arg) {
#define HANDLE_LOCAL(Type)                                                                  \
	variant_case(vm::opargs::Type, local) {                                                 \
		if (!local_offset_map.contains(local.var_name)) throw InvalidLocalNameError(local); \
	}
				FOR_EACH(HANDLE_LOCAL, VM_OPARG_LOCAL_TYPES);
#undef HANDLE_LOCAL
			}
		}
	}
}

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	using namespace instructions;
	validateExtension(instruction);
	validateLocalArgs(instruction);

	variant_match(instruction) {
		variant_case(Op_init_lany_type, instr) {
			validateArgInstantiable(instr.arg1);
			pushStackState(instr.arg0, instr.arg1);
		}
		variant_case(Op_alloc_lptr_type, instr) { validateArgInstantiable(instr.arg1); }
		variant_case(Op_upcast_lptr_lptr, instr) {
			// @TODO implement checking if the cast is valid after #732
		}
		variant_case(Op_deinit, instr) { handleDeinit(); }
		variant_case(Op_label, label) { handleLabel(label); }
		variant_case(Op_call_func, func) { handleCall(func.arg0.function_name); }
		variant_case(Op_call_builtin_func, func) { handleCall(func.arg0.function_name); }
		variant_case(Op_jmp_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(Op_jmpIf_label, jmp) {
			saveStackState(jmp.arg0.label_name);
			label_users[jmp.arg0.label_name].emplace_back(jmp);
		}
		variant_case(Op_jmpIfNot_label, jmp) {
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

FunctionBuilder::FunctionBuilder(
	vm::code::Identifier name, const GlobalDataMap& globals, const TypeContext& types
):
	  name(name),
	  type_context(types),
	  globals(globals) {
	TypeCRef func_result_type = type_context.getMetadata()
	                                .atMaybe(name.str)
	                                .expect<MissingFunctionalTypeError>(name.str)
	                                ->getResultType()
	                                .expect<TypeIsNotFunctionalError>(name);
	// TODO: This is strange, maybe should be ifed.
	pushStackState(base::StrID("ret_val"), func_result_type->getName());
	auto param_types = type_context.getMetadata().at(name)->getParameters().value();
	for (auto [index, type]: std::views::enumerate(param_types))
		pushStackState(base::StrID(base::strConcat("arg", index).c_str()), type->getName());
}

usize vm::code::builders::FunctionBuilder::pushStackState(
	vm::opargs::StackLocalAny name, vm::opargs::Type type
) {
	const usize type_size = type_context.getMetadata()
	                            .atMaybe(type.type_name)
	                            .expect<UnknownTypeError>(type)
	                            ->getSize();

	if (local_offset_map.contains(name.var_name)) throw DuplicateLocalNameError(name);

	usize offset = 0;

	if (!local_stack.empty()) {
		const LocalStackEntry& prev_entry = local_stack.back();
		offset                            = prev_entry.local_stack_position + prev_entry.type_size;
	}

	local_offset_map.put(name.var_name, offset);
	local_stack.emplace_back(LocalStackEntry{ .unique_id            = LocalStackEntryID::next(),
	                                          .type_name            = type.type_name,
	                                          .local_stack_position = offset,
	                                          .type_size            = type_size });

	max_stack_size = std::max(max_stack_size, offset + type_size);

	return offset;
}

void FunctionBuilder::initType(instructions::Op_init_lany_type init) {
	instructions.emplace_back(init);
	pushStackState(init.arg0, init.arg1);
}

void FunctionBuilder::handleDeinit() {
	if (local_stack.empty()) throw EmptyStackDeinitError();

	TypeCRef func_result_type = *type_context.getMetadata().at(name.str)->getResultType();
	if (func_result_type->getSize() > 0 && local_stack.size() == 1) throw ReturnValueDeinitError();

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
		if (tp->getName() != local_stack.at(local_stack.size() - param_count + i).type_name)
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
	auto top = local_stack.empty() ? base::Optional<LocalStackEntry>{} : local_stack.back();
	if (stack_top_at_label.contains(at_label.label_name)) {
		if (stack_top_at_label[at_label.label_name] != top)
			throw builders::StackStructureMismatchError(label_users.at(at_label.label_name));
	} else {
		stack_top_at_label.put(at_label.label_name, top);
		label_users.put(at_label.label_name, {});
	}
}

void vm::code::builders::FunctionBuilder::handleRet() {
	if (local_stack.empty()
	    || local_stack.at(0).type_name
	           != type_context.getMetadata().at(name.str)->getResultType().value()->getName()) {
		throw BadReturnError();
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
	for (const auto& type: types) {
		tctx.metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));
		tctx.types.push_back(type);
	}
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
