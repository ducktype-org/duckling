
#include "query_type_symbol_data.hpp"

#include "helios/tsh/queries/types.hpp"
#include "helios/tsh/type_interface.hpp"
#include "symbol_kind.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/class.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "lang_definitions/key_spec_op.hpp"
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

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

		struct SpecifiersResult {
			base::Optional<tsh::MemberVisibility> visibility_opt;
			bool                                  is_static;
		};

		static SpecifiersResult getSpecifiers(query::Context& ctx, SymID sym) {
			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(sym);
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
				if (keyword == lang_def::Keyword::Private)
					visibility_opt = tsh::MemberVisibility::Public;
				if (keyword == lang_def::Keyword::Public)
					visibility_opt = tsh::MemberVisibility::Private;
				if (keyword == lang_def::Keyword::Private)
					visibility_opt = tsh::MemberVisibility::Protected;
				if (keyword == lang_def::Keyword::Static) is_static = true;
			}
			return { .visibility_opt = visibility_opt, .is_static = is_static };
		}

		static auto provide(Context& ctx, QKey sym) -> PResult {
			CORE_ASSERT(kind(sym) == SymbolKind::Class, "Symbol is not a class");
			tsh::ClassAbstractType    class_type = ctx.query<tsh::QueryClassType>(sym);
			ClassSymbolData           result{};
			tsh::TypeInterfaceBuilder interface_builder(class_type);

			auto class_stmt       = getSymRef(sym)->stmtCast(ctx).value();
			auto class_body_scope = queryBodyCodeScopeFor(ctx, class_stmt);
			Ref  class_symbols = &ctx.query<QuerySymbolsInScope>(class_body_scope)->valueOrThrow();

			std::vector<SpecifiersResult> members_specifiers
				= (*class_symbols)
			    | std::views::transform([&](SymID sym) { return getSpecifiers(ctx, sym); })
			    | std::ranges::to<std::vector>();

			tsh::MemberVisibility default_visiblity = tsh::MemberVisibility::Public;
			if (std::ranges::any_of(members_specifiers, [](SpecifiersResult spec) {
					return spec.visibility_opt.has_value();
				}))
				default_visiblity = tsh::MemberVisibility::Private;

			ClassSymbolData class_info;
			for (usize i{ 0 }; i < (*class_symbols).size(); i++) {
				auto sym        = (*class_symbols)[i];
				auto specifiers = members_specifiers[i];
				switch (kind(sym)) {
				case SymbolKind::Method:
					interface_builder.push({
						sym,
						class_type,
					}) break;
				case SymbolKind::Constructor:
					class_info.constructors.push_back(sym);
					break;
				case SymbolKind::Destructor:
					// This doesn't catch multiple destructors
					class_info.destructor = sym;
					break;
				case SymbolKind::Field:
					class_info.members.push_back(sym);
					break;
				default:
					throw base::NotYetImplemented(base::strConcat(
						"Using ", typeid(kind(sym)).name(), " inside a class is not yet implemented."
					));
				}
			}
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
