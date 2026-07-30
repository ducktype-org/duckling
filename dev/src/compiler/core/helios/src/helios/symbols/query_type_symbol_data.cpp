
#include "query_type_symbol_data.hpp"

#include "symbol_kind.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/class.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/errors/inherited_type_with_specifiers.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

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

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Class, "Symbol is not a class");

			auto class_stmt = getSymRef(key)->stmtCast(ctx).value();

			auto class_body_scope = queryBodyCodeScopeFor(ctx, class_stmt);
			Ref  class_symbols = &ctx.query<QuerySymbolsInScope>(class_body_scope)->valueOrThrow();

			ClassSymbolData class_info;
			for (auto sym: *class_symbols) {
				switch (kind(sym)) {
				case SymbolKind::Method:
					class_info.methods.push_back(sym);
					break;
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

			// A class inherits from a plain type, so specifiers (`ref`, `box`, `const`, ...) on
			// the extended class or on an implemented interface are an error.
			const auto check_no_specifiers
				= [&](const tsh::SymbolType<>&                                inherited,
			          const pst::AccessLocked<pst::ExprElement>               expr,
			          const InheritedTypeWithSpecifiersError::InheritanceKind inheritance_kind
			      ) -> base::OkBad {
				if (inherited.isPlainAbstractType()) return base::OK;
				ctx.logInt(makeBox<InheritedTypeWithSpecifiersError>(
					ctx,
					expr.unlock(ctx)->getStablePosition(),
					class_info.name.str(),
					inheritance_kind,
					inherited
				));
				return base::BAD;
			};

			if_opt_some(class_data_parser.base_class, base) {
				UNPACK_QRESULT(auto ctv =, getTypeCTVFromPST(ctx, base));
				const auto base_type = *ctv.get<tsh::SymbolType<>>();

				auto specifiers = check_no_specifiers(
					base_type, base, InheritedTypeWithSpecifiersError::InheritanceKind::ExtendedClass
				);
				if (specifiers.isBad()) return query::Failed();

				class_info.base = base_type.getType();
				class_info.implements.push_back(base_type.getType());
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: *implements.unlock(ctx)) {
					const auto interface_expr = interface.unlock(ctx)->getExpr();
					UNPACK_QRESULT(auto ctv =, getTypeCTVFromPST(ctx, interface_expr));
					const auto interface_type = ctv.get<tsh::SymbolType<>>().value();
					auto       specifiers     = check_no_specifiers(
                        interface_type,
                        interface_expr,
                        InheritedTypeWithSpecifiersError::InheritanceKind::ImplementedInterface
                    );
					if (specifiers.isBad()) return query::Failed();
					class_info.implements.push_back(interface_type.getType());
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

	struct IMPLEMENT_QUERY(QueryDynamicArrayTypeData, DynamicArrayTypeData) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			// The fields of a dynamic array type are always:
			// - `ptr` - to the start of the data
			// - `len` - length of the list
			// - `off_start_reserved` - offset of the start of reserved memory
			// - `off_end_reserved` - offset of the end of reserved memory
			SymID ptr = ctx.query<defgen::QueryGeneratedSymbol>(
				{ .name                  = base::StrID{ "ptr" },
			      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = 0 } }
			);
			SymID len = ctx.query<defgen::QueryGeneratedSymbol>(
				{ .name                  = base::StrID{ "len" },
			      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = 1 } }
			);
			SymID off_start_reserved = ctx.query<defgen::QueryGeneratedSymbol>(
				{ .name                  = base::StrID{ "off_start_reserved" },
			      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = 2 } }
			);
			SymID off_end_reserved = ctx.query<defgen::QueryGeneratedSymbol>(
				{ .name                  = base::StrID{ "off_end_reserved" },
			      .generated_symbol_data = defgen::Field{ .parent_type = key, .index = 3 } }
			);

			return DynamicArrayTypeData{
				.ptr                = ptr,
				.len                = len,
				.off_start_reserved = off_start_reserved,
				.off_end_reserved   = off_end_reserved,
			};
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDynamicArrayTypeData);
}
