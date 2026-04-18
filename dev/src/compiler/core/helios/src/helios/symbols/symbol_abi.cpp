

#include "symbol_abi.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/call_argument.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/specifier_block.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {


	/**
	 * @brief Extracts a string literal from an extern() call argument.
	 * @param ctx The query context.
	 * @param arg The call argument to extract the string from.
	 * @return The string literal value, or a query::Failed if extraction fails (there is no string
	 * literal there).
	 */
	query::QResult<base::StrID> getStrFromCallArg(
		query::Context& ctx, pst::AccessLocked<pst::CallArgument> arg
	) {
		auto call_arg = arg.unlock(ctx);

		if (call_arg->getArgName().value.has_value())
			throw base::NotYetImplemented("Naming arguments in extern() is not supported yet.");

		auto expr_holder = call_arg->getArg().unlock(ctx).dynamicCast<pst::ExprHolder>().value();

		auto str_lit_opt
			= expr_holder->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();

		match_optional(str_lit_opt) {
			opt_none {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Expected string literal in extern() call argument",
					arg.unlock(ctx)->getSourcePosition().unlock(ctx)
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
		std::vector<pst::AccessLocked<pst::CallArgument>> args{ extern_args.unlock(ctx)->begin(),
			                                                    extern_args.unlock(ctx)->end() };

		if (args.empty()) {
			ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				"extern() requires at least one argument specifying the ABI",
				extern_args.unlock(ctx)->getSourcePosition().unlock(ctx)
			));
			return query::Failed();
		}

		UNPACK_QRESULT(auto first_arg =, getStrFromCallArg(ctx, args[0]));

		if (first_arg == base::StrID("C")) {
			if (args.size() == 2) {  // `extern("C" "mylib")` case
				UNPACK_QRESULT(auto lib_str_lit =, getStrFromCallArg(ctx, args[1]));
				return CAbi{ .library = lib_str_lit };
			} else if (args.size() == 1) {  // `extern("C")` case
				return CAbi{};
			} else {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Too many arguments for C ABI in extern()",
					extern_args.unlock(ctx)->getSourcePosition().unlock(ctx)
				));
				return query::Failed();
			}
		} else {
			ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				"Unsupported ABI specified in extern()",
				extern_args.unlock(ctx)->getSourcePosition().unlock(ctx)
			));
			return query::Failed();
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolABI, QuerySymbolABI_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto sym_ref = getSymRef(key);

			// Builtin functions are implemented in C/C++ and use the C ABI.
			variant_match(sym_ref->other) {
				variant_case_novalue(builtin::BuiltinFunctionData) { return CAbi{}; }
				variant_case(defgen::GeneratedSymbolData, gen_data) {
					variant_match(gen_data.data) {
						variant_case_novalue(defgen::GeneratedSymbolData::BuiltinOperator) {
							return CAbi{};
						}
					}
				}
			}

			// @TODO: #895 fix it when we add script based package targets
			if (name(key) == "main" && isGlobalFun(key)) {
				// main is not mangled
				return CAbi{};
			}

			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(key);
			for (auto specifier: *specifiers) {
				if (specifier.unlock(ctx)->getSpecifier() == pst::Keyword::Extern) {
					match_optional(specifier.unlock(ctx)->getArgs()) {
						opt_some(args) { return getSymbolABI(ctx, args); }
						opt_none {
							ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
								"extern symbol requires ABI specification passed as an argument",
								specifier.unlock(ctx)->getSourcePosition().unlock(ctx)
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
