
#include "query_type_symbol_data.hpp"

#include "helios/symbols/query_type_of_symbol.hpp"
#include "helios/tsh/queries/types.hpp"
#include "helios/tsh/type_interface.hpp"
#include "symbol_kind.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/copy_constructor.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/class.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "lang_definitions/key_spec_op.hpp"
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	ClassMemberSpecifiersResult getClassMemberSpecifiers(query::Context& ctx, SymID sym) {
		auto                                  specifiers = ctx.query<QuerySpecifiersOfSymbol>(sym);
		base::Optional<tsh::MemberVisibility> visibility_opt{};
		bool                                  is_static = false;

		auto is_visiblity_keyword = [](lang_def::Keyword keyword) {
			return keyword == lang_def::Keyword::Private || keyword == lang_def::Keyword::Public
			    || keyword == lang_def::Keyword::Protected;
		};
		auto is_static_keyword
			= [](lang_def::Keyword keyword) { return keyword == lang_def::Keyword::Static; };

		for (auto specifier_locked: *specifiers) {
			auto specifier = specifier_locked.unlock(ctx);
			auto keyword   = specifier->getSpecifier().unlock(ctx)->unwrap();

			if (is_visiblity_keyword(keyword) and visibility_opt.has_value())
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Class visibility specifier is duplicated with another one.",
					specifier->getStablePosition()
				));
			if (is_static_keyword(keyword) and is_static)
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Class static specifier is duplicated with another one.",
					specifier->getStablePosition()
				));
			if (keyword == lang_def::Keyword::Public)
				visibility_opt = tsh::MemberVisibility::Public;
			if (keyword == lang_def::Keyword::Protected)
				visibility_opt = tsh::MemberVisibility::Protected;
			if (keyword == lang_def::Keyword::Private)
				visibility_opt = tsh::MemberVisibility::Private;
			if (keyword == lang_def::Keyword::Static) is_static = true;
		}
		return { .visibility_opt = visibility_opt, .is_static = is_static };
	}

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstVisitorPanicky {
			query::Context& ctx;

			ClassDataParser(query::Context& ctx): ctx(ctx) {}

			base::Optional<base::StrID>                            name;
			base::Optional<pst::AccessLocked<pst::ExprElement>>    base_class;
			base::Optional<pst::AccessLocked<pst::ImplementsList>> implements;

			void visitClass(pst::Access<pst::Class> stmt) final {
				name = stmt->getName().unlock(ctx)->unwrap();
				if (auto base = stmt->getBase().unlockOpt(ctx))
					base_class = base.value()->getExpr().unlock(ctx);
				if (auto implements = stmt->getImplements().unlockOpt(ctx))
					this->implements = implements.value();
			}
		};

		static tsh::MemberVisibility getDefaultMemberVisibility(
			std::vector<ClassMemberSpecifiersResult> members_specifiers
		) {
			if (std::ranges::any_of(members_specifiers, [](ClassMemberSpecifiersResult spec) {
					return spec.visibility_opt.has_value();
				}))
				return tsh::MemberVisibility::Private;
			return tsh::MemberVisibility::Public;
		}

		static tsh::InterfaceElement::InterfaceElementKind getElementKind(
			SymbolKind kind, ClassMemberSpecifiersResult specifiers
		) {
			using ElementKind = tsh::InterfaceElement::InterfaceElementKind;
			switch (kind) {
			case SymbolKind::Method:
				if (specifiers.is_static) return ElementKind::StaticMethod;
				return ElementKind::Method;
			case SymbolKind::Destructor:
				return ElementKind::Method;
			case SymbolKind::Field:
				if (specifiers.is_static) return ElementKind::StaticField;
				return ElementKind::Field;
			case SymbolKind::Constructor:
				// The only constructor now is a copy constructor which is a static method
				return ElementKind::StaticMethod;
			default:
				return ElementKind::Other;
			}
		}

		static tsh::MemberSpecialKind specialKind(query::Context& ctx, SymID sym) {
			switch (kind(sym)) {
			case SymbolKind::Destructor:
				return tsh::MemberSpecialKind::UserDestructor;
			case SymbolKind::Constructor:
				CORE_ASSERT(
					name(sym) == "copy",
					"The only supported constructor in the class is copy constructor"
				);
				return tsh::MemberSpecialKind::CopyConstructor;
			case SymbolKind::Method:
				if (name(sym) == "toString") {
					const auto method_type
						= ctx.query<helios::QueryTypeOfSymbol>(sym)->valueOrThrow().getType();
					if (method_type.getKind() == tsh::Kind::Function) {
						const auto fn_type = method_type.as<tsh::FunctionAbstractType>();
						// parameterTypes.size() == 1 means method has not params except self.
						if (fn_type.getParameterTypes().size() == 1
						    && fn_type.getResultType()
						           == tsh::SymbolType<>::withDefaults(tsh::getStringType(ctx)))
							return tsh::MemberSpecialKind::ToString;
					}
				}
				return tsh::MemberSpecialKind::None;
			default:
				return tsh::MemberSpecialKind::None;
			}
		}

		/**
		 * @brief The members of a class that change how it is constructed, copied and destroyed.
		 * @note These are collected from the members of the class rather than from the `defgen`
		 * helpers, because those ask for the `ClassSymbolData` that is being built here.
		 */
		struct UserDefinedMembers {
			bool has_copy_constructor = false;
			bool has_destructor       = false;
		};

		static void collectUserDefinedMember(
			tsh::MemberSpecialKind special_kind, UserDefinedMembers& user_members
		) {
			if (special_kind == tsh::MemberSpecialKind::CopyConstructor)
				user_members.has_copy_constructor = true;
			if (special_kind == tsh::MemberSpecialKind::UserDestructor)
				user_members.has_destructor = true;
		}

		/**
		 * @brief Whether the field is declared with an initializing value, as in `x: i32 = 2;`.
		 */
		static bool hasInitializer(query::Context& ctx, const tsh::InterfaceElement& field) {
			auto field_pst = maybeSymbolPst(field.getSymbol())
			                     .value()
			                     .unlock(ctx)
			                     .dynamicCast<pst::Field>()
			                     .value();
			return field_pst->getInit().has_value();
		}

		/**
		 * @brief A class is default constructible when each of its fields either has an
		 * initializing value, or is of a default constructible type.
		 */
		static bool isDefaultConstructible(
			query::Context& ctx, const tsh::TypeInterface& class_interface
		) {
			return std::ranges::all_of(
				class_interface.getFieldsView(),
				[&](const tsh::InterfaceElement& field) {
					return hasInitializer(ctx, field)
				        or field.getType(ctx).isDefaultConstructible(ctx);
				}
			);
		}

		/**
		 * @brief A class is trivially zero initializable when none of its fields has an
		 * initializing value, and all of them are trivially zero initializable.
		 */
		static bool isTriviallyZeroInitializable(
			query::Context& ctx, const tsh::TypeInterface& class_interface
		) {
			return std::ranges::all_of(class_interface.getFieldsView(), [&](const auto& field) {
				return not hasInitializer(ctx, field)
				   and field.getType(ctx).isTriviallyZeroInitializable(ctx);
			});
		}

		/**
		 * @brief A user-defined copy constructor makes the class copyable regardless of its
		 * fields, otherwise all of its fields have to be copyable.
		 */
		static bool isCopyable(
			query::Context&           ctx,
			const tsh::TypeInterface& class_interface,
			UserDefinedMembers        user_members
		) {
			if (user_members.has_copy_constructor) return true;

			return std::ranges::all_of(class_interface.getFieldsView(), [&](const auto& field) {
				return field.getType(ctx).isCopyable(ctx);
			});
		}

		/**
		 * @brief A user-defined copy constructor means copies must run user code, so the class is
		 * never trivially copyable. Otherwise all of its fields have to be trivially copyable.
		 */
		static bool isTriviallyCopyable(
			query::Context&           ctx,
			const tsh::TypeInterface& class_interface,
			UserDefinedMembers        user_members
		) {
			if (user_members.has_copy_constructor) return false;

			return std::ranges::all_of(class_interface.getFieldsView(), [&](const auto& field) {
				return field.getType(ctx).isTriviallyCopyable(ctx);
			});
		}

		/**
		 * @brief A user-defined destructor means destruction runs user code, so the class is never
		 * trivially destructible. Otherwise the destructor is a no-op only when all of the fields
		 * are trivially destructible.
		 */
		static bool isTriviallyDestructible(
			query::Context&           ctx,
			const tsh::TypeInterface& class_interface,
			UserDefinedMembers        user_members
		) {
			if (user_members.has_destructor) return false;

			return std::ranges::all_of(class_interface.getFieldsView(), [&](const auto& field) {
				return field.getType(ctx).isTriviallyDestructible(ctx);
			});
		}

		/**
		 * @brief A class carries information when it declares at least one field.
		 */
		static bool carriesInformation(const tsh::TypeInterface& class_interface) {
			// this should probably be changed/expanded in the future:
			auto fields = class_interface.getFieldsView();
			return fields.begin() != fields.end();
		}

		/**
		 * @brief Push the compiler-generated constructors of the class into its interface.
		 */
		static void pushConstructors(
			query::Context&            ctx,
			tsh::TypeInterfaceBuilder& interface_builder,
			tsh::ClassAbstractType     class_type,
			bool                       is_default_constructible
		) {
			using ElementKind = tsh::InterfaceElement::InterfaceElementKind;

			auto generated_constructor = [&](defgen::Constructor::Kind kind) {
				return ctx.query<defgen::QueryGeneratedSymbol>({
					.name = name(class_type.getSymbol()),
					.generated_symbol_data = defgen::Constructor{ .type = class_type, .kind = kind },
				});
			};

			interface_builder.push(
				generated_constructor(defgen::Constructor::Kind::Implicit),
				ElementKind::StaticMethod,
				tsh::MemberVisibility::Public,
				tsh::MemberSpecialKind::Constructor
			);

			if (not is_default_constructible) return;

			interface_builder.push(
				generated_constructor(defgen::Constructor::Kind::Default),
				ElementKind::StaticMethod,
				tsh::MemberVisibility::Public,
				tsh::MemberSpecialKind::ParameterlessConstructor
			);
		}

		static auto provide(Context& ctx, QKey sym) -> PResult {
			CORE_ASSERT(kind(sym) == SymbolKind::Class, "Symbol is not a class");
			tsh::ClassAbstractType    class_type = ctx.query<tsh::QueryClassType>(sym);
			tsh::TypeInterfaceBuilder interface_builder(class_type);

			auto class_stmt       = getSymRef(sym)->stmtCast(ctx).value();
			auto class_body_scope = queryBodyCodeScopeFor(ctx, class_stmt);
			Ref  class_symbols = &ctx.query<QuerySymbolsInScope>(class_body_scope)->valueOrThrow();

			std::vector<ClassMemberSpecifiersResult> members_specifiers
				= (*class_symbols) | std::views::transform([&](SymID member_sym) {
					  return getClassMemberSpecifiers(ctx, member_sym);
				  })
			    | std::ranges::to<std::vector>();

			auto default_visiblity = getDefaultMemberVisibility(members_specifiers);

			ClassSymbolData    class_info;
			UserDefinedMembers user_members;
			for (usize i{ 0 }; i < (*class_symbols).size(); i++) {
				auto member_sym          = (*class_symbols)[i];
				auto specifiers          = members_specifiers[i];
				auto interface_elem_kind = getElementKind(kind(member_sym), specifiers);
				auto member_visibility   = specifiers.visibility_opt.copyValueOr(default_visiblity);
				auto member_special_kind = specialKind(ctx, member_sym);

				collectUserDefinedMember(member_special_kind, user_members);

				interface_builder.push(
					member_sym, interface_elem_kind, member_visibility, member_special_kind
				);
			}

			auto tmp_interface = interface_builder.build();

			class_info.is_default_constructible = isDefaultConstructible(ctx, tmp_interface);
			class_info.is_trivially_zero_initializable
				= isTriviallyZeroInitializable(ctx, tmp_interface);
			class_info.is_copyable = isCopyable(ctx, tmp_interface, user_members);
			class_info.is_trivially_copyable
				= isTriviallyCopyable(ctx, tmp_interface, user_members);
			class_info.is_trivially_destructible
				= isTriviallyDestructible(ctx, tmp_interface, user_members);
			class_info.carries_information = carriesInformation(tmp_interface);

			pushConstructors(
				ctx, interface_builder, class_type, class_info.is_default_constructible
			);

			class_info.declared_interface = interface_builder.build();

			// Find the name
			auto class_data_parser = ClassDataParser(ctx);
			class_stmt->acceptVisitor(class_data_parser);
			class_info.name = class_data_parser.name.value();

			if_opt_some(class_data_parser.base_class, base) {
				UNPACK_QRESULT(auto ctv =, getTypeCTVFromPST(ctx, base));
				// @TODO: #1630 Raise errors, here, or preferably earlier, if the symbol
				// type of the base class has any specifiers.
				class_info.base = ctv.get<tsh::SymbolType<>>()->getType();
				class_info.implements.push_back(ctv.get<tsh::SymbolType<>>()->getType());
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: *implements.unlock(ctx)) {
					UNPACK_QRESULT(
						auto ctv =, getTypeCTVFromPST(ctx, interface.unlock(ctx)->getExpr())
					);
					// @TODO: #1630 Raise errors, here, or preferably earlier, if the symbol
					// type of the base class has any specifiers.
					class_info.implements.push_back(ctv.get<tsh::SymbolType<>>()->getType());
				}
			}

			return class_info;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);

	struct IMPLEMENT_QUERY(QueryTupleTypeData, QueryTupleTypeData_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			TupleTypeData tuple_info;

			const auto& components = key.getComponents();
			tuple_info.members.reserve(components.size());
			for (usize order = 0; order < components.size(); order++) {
				tuple_info.members.push_back(ctx.query<defgen::QueryGeneratedSymbol>(
					{ // Tuple field names are _1, _2, ...
				      // Starting from 1, not 0!
				      .name                  = base::StrID{ base::strConcat("_", order + 1) },
				      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = order } }
				));
			}

			return tuple_info;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTupleTypeData);

	struct IMPLEMENT_QUERY(QuerySliceTypeData, SliceTypeData) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			// The fields of a slice type are always `ptr` and `len`, in that order.
			SymID ptr = ctx.query<defgen::QueryGeneratedSymbol>(
				{ .name                  = base::StrID{ "ptr" },
			      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = 0 } }
			);
			SymID len = ctx.query<defgen::QueryGeneratedSymbol>(
				{ .name                  = base::StrID{ "len" },
			      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = 1 } }
			);

			return SliceTypeData{ .ptr = ptr, .len = len };
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySliceTypeData);

}
