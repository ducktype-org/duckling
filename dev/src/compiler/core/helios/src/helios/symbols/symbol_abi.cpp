
#include "symbol_abi.hpp"

#include "helios_private/symbols/symbols.hpp"

#include <helios_private/symbols/symbol_data.hpp>
#include <pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <pst_parser/elements/hierarchy/statements/stmt_specifier.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <pst_parser/lang_parser_element.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::helios {

	QuerySymbolABI_Result getSymbolABI(
		query::Context& ctx, pst::AccessLocked<pst::CallList> extern_args
	) {
		std::vector<pst::AccessLocked<pst::LangElement>> args{ extern_args.unlock(ctx)->begin(),
			                                                   extern_args.unlock(ctx)->end() };
		
		auto expr_holder_opt = args[0].unlock(ctx).dynamicCast<pst::ExprHolder>();
		if (not expr_holder_opt) return query::QError(errors::Failed());

		auto str_lit_opt = expr_holder_opt.value()->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();
		if (not str_lit_opt) return query::QError(errors::Failed());

		auto str = str_lit_opt.value()->getValue();
		if (str.value == base::StrID("C")) {
			if (args.size() == 2) { // `extern("C" "mylib")` case
				auto lib_expr_holder_opt = args[1].unlock(ctx).dynamicCast<pst::ExprHolder>();
				if (not lib_expr_holder_opt) return query::QError(errors::Failed());
				
				auto lib_str_lit_opt = lib_expr_holder_opt.value()->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();
				if (not lib_str_lit_opt) return query::QError(errors::Failed());
				
				return CAbi{ .library = lib_str_lit_opt.value()->getValue().value };
			}
			return CAbi{};
		} else {
			return query::QError(errors::Failed());
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolABI, QuerySymbolABI_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(key);
			for (auto specifier: *specifiers) {
				if (specifier.unlock(ctx)->getSpecifier() == pst::Keyword::Extern) {
					auto args = specifier.unlock(ctx)->getArgs();
					if (not args) return query::QError(errors::Failed());

					return getSymbolABI(ctx, args.value());
				}
			}
			return DefaultAbi{};
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolABI)
}
