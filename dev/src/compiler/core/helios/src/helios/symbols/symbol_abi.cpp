

#include "symbol_abi.hpp"

#include <diagnostic_interactive/placeholder.hpp>
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

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/query_impl.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {


	query::QResult<base::StrID> getStrFromExternCallArg(
		query::Context& ctx, pst::AccessLocked<pst::LangElement> arg
	) {
		// Add proper helios error handling once we have new error logging system
		auto call_arg = arg.unlock(ctx).dynamicCast<pst::CallArgument>().value();

		if (call_arg->getArgName().value.has_value())
			throw base::NotYetImplemented("Naming arguments in extern() is not supported yet.");

		auto expr_holder = call_arg->getArg().unlock(ctx).dynamicCast<pst::ExprHolder>().value();

		auto str_lit_opt
			= expr_holder->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();

		match_optional(str_lit_opt) {
			opt_none {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Expected string literal in extern() call argument",
					arg.unlock(ctx)->getSourcePosition()
				));
				return query::Failed();
			}
			opt_some(str_lit) { return str_lit->getValue().value; }
		}
		return query::Failed();  // unreachable
	}

	QuerySymbolABI_Result getSymbolABI(
		query::Context& ctx, pst::AccessLocked<pst::CallList> extern_args
	) {
		std::vector<pst::AccessLocked<pst::LangElement>> args{ extern_args.unlock(ctx)->begin(),
			                                                   extern_args.unlock(ctx)->end() };

		if (args.empty()) {
			ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				"extern() requires at least one argument specifying the ABI",
				extern_args.unlock(ctx)->getSourcePosition()
			));
			return query::Failed();
		}

		UNPACK_QRESULT(auto first_arg =, getStrFromExternCallArg(ctx, args[0]));

		if (first_arg == base::StrID("C")) {
			if (args.size() == 2) {  // `extern("C" "mylib")` case
				UNPACK_QRESULT(auto lib_str_lit =, getStrFromExternCallArg(ctx, args[1]));
				return CAbi{ .library = lib_str_lit };
			} else if (args.size() == 1) {  // `extern("C")` case
				return CAbi{};
			} else {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Too many arguments for C ABI in extern()",
					extern_args.unlock(ctx)->getSourcePosition()
				));
				return query::Failed();
			}
		} else {
			ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				"Unsupported ABI specified in extern()", extern_args.unlock(ctx)->getSourcePosition()
			));
			return query::Failed();
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolABI, QuerySymbolABI_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(key);
			for (auto specifier: *specifiers) {
				if (specifier.unlock(ctx)->getSpecifier() == pst::Keyword::Extern) {
					match_optional(specifier.unlock(ctx)->getArgs()) {
						opt_some(args) { return getSymbolABI(ctx, args); }
						opt_none {
							ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
								"extern symbol requires ABI specification passed as an argument",
								specifier.unlock(ctx)->getSourcePosition()
							));
							return query::Failed();
						}
					}
				}
			}
			return DefaultAbi{};
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolABI)
}
