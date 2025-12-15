

#include "symbol_abi.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/call_argument.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/stmt_specifier.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::helios {


	query::QResult<base::StrID, query::Failed> getStrFromExternCallArg(
		query::Context& ctx, pst::AccessLocked<pst::LangElement> arg
	) {
		// Add proper helios error handling once we have new error logging system
		auto call_arg_opt = arg.unlock(ctx).dynamicCast<pst::CallArgument>();
		if (!call_arg_opt) return query::QError(query::Failed());

		if (call_arg_opt.value()->getArgName().value.has_value())
			throw base::NotYetImplemented("Naming arguments in extern() is not supported yet.");

		auto expr_holder_opt
			= call_arg_opt.value()->getArg().unlock(ctx).dynamicCast<pst::ExprHolder>();
		if (!expr_holder_opt) return query::QError(query::Failed());

		auto str_lit_opt
			= expr_holder_opt.value()->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();
		if (!str_lit_opt) return query::QError(query::Failed());

		return str_lit_opt.value()->getValue().value;
	}

	QuerySymbolABI_Result getSymbolABI(
		query::Context& ctx, pst::AccessLocked<pst::CallList> extern_args
	) {
		std::vector<pst::AccessLocked<pst::LangElement>> args{ extern_args.unlock(ctx)->begin(),
			                                                   extern_args.unlock(ctx)->end() };

		if (args.empty()) return query::QError(query::Failed());

		auto first_arg_result = getStrFromExternCallArg(ctx, args[0]);
		if (first_arg_result.hasError()) return query::QError(first_arg_result.error());

		if (first_arg_result.valueOrThrow() == base::StrID("C")) {
			if (args.size() == 2) {  // `extern("C" "mylib")` case
				auto lib_str_lit_opt = getStrFromExternCallArg(ctx, args[1]);
				if (lib_str_lit_opt.hasError()) return query::QError(lib_str_lit_opt.error());

				return CAbi{ .library = lib_str_lit_opt.valueOrThrow() };
			}
			return CAbi{};
		} else {
			// @TODO add proper diagnostic here for invalid ABI
			return query::QError(query::Failed());
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolABI, QuerySymbolABI_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(key);
			for (auto specifier: *specifiers) {
				if (specifier.unlock(ctx)->getSpecifier() == pst::Keyword::Extern) {
					auto args = specifier.unlock(ctx)->getArgs();
					if (not args) return query::QError(query::Failed());

					return getSymbolABI(ctx, args.value());
				}
			}
			return DefaultAbi{};
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolABI)
}
