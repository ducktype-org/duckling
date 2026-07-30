

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
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/attributes.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <tsl/c_abi_converter.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <span>

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

		if (call_arg->getArgName())
			throw base::NotYetImplemented("Naming arguments in extern() is not supported yet.");

		auto expr_holder = call_arg->getArg().unlock(ctx).dynamicCast<pst::ExprHolder>().value();

		auto str_lit_opt
			= expr_holder->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();

		match_optional(str_lit_opt) {
			opt_none {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected string literal in extern() call argument",
					arg.unlock(ctx)->getStablePosition()
				));
				return query::Failed();
			}
			opt_some(str_lit) { return str_lit->getValue().value; }
		}
		return query::Failed();  // unreachable
	}

	namespace {
		/**
		 * @brief Whether a type is valid as variadic parameter type in  `@cffi_variadic_after`
		 * declaration.
		 * @return Empty value if valid, and the value that should be declared instead if invalid.
		 */
		std::expected<std::monostate, tsh::AbstractType> validateVariadicArgType(
			query::Context& ctx, tsh::AbstractType type
		) {
			using enum tsh::IntegralAbstractType::Signedness;
			using enum tsh::Kind;
			switch (type.getKind()) {
			case Bool:
			case Char:
			case Byte:
				return std::unexpected(tsh::getIntegralType(ctx, 64, Signed));
			case Integral: {
				auto int_type = type.as<tsh::IntegralAbstractType>();
				if (int_type.getSize() >= Bits(32)) return {};
				return std::unexpected(tsh::getIntegralType(ctx, 32, int_type.getSignedness()));
			}
			case Float: {
				auto float_type = type.as<tsh::FloatAbstractType>();
				if (float_type.getSize() == Bits(64)) return {};
				return std::unexpected(tsh::getFloatType(ctx, 64));
			}
			default:
				return {};
			}
		}

		/**
		 * @brief Validates variadic function declaration.
		 */
		query::QResult<u64> validateVariadicAfter(
			query::Context& ctx, const HOUTFunctionDeclaration& decl, u64 fixed_params
		) {
			if (fixed_params == 0) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Variadic function declaration should have at least one fixed parameter.",
					decl.origin
				));
				return query::Failed();
			}

			if (fixed_params >= decl.parameters.size()) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Variadic function declaration should have at least one variadic parameter.",
					decl.origin
				));
				return query::Failed();
			}

			// Ignoring reference kind, it will be checked later, reference types in CFFI are
			// invalid. Only the variadic parameters are subject to the default argument promotions.
			for (auto& param: std::span(decl.parameters).subspan(fixed_params)) {
				auto param_result = validateVariadicArgType(ctx, param.type.getType());
				if (not param_result.has_value()) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Variadic function parameter has to be promoted. Got ",
							"`",
							param.type.toString(),
							"`, expected `",
							param_result.error().toString(),
							"`."
						),
						param.origin
					));
					return query::Failed();
				}
			}

			return fixed_params;
		}

		/**
		 * @brief Validate if the parameters
		 */
		query::QResult<std::monostate> checkCABIParamTypes(
			query::Context& ctx, const HOUTFunctionDeclaration& decl
		) {
			for (auto& param: decl.parameters) {
				UNPACK_QRESULT_CREF(auto& result =, ctx.query<tsl::QueryCAbiTypeOf>(param.type));
				if (not result.has_value()) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"CABI function declaration has invalid parameter. Reason: `",
							result.error(),
							"`."
						),
						param.origin
					));
					return query::Failed();
				}
			}
			return {};
		}
	}

	QuerySymbolABI_Result getSymbolABI(
		query::Context& ctx, SymID sym, pst::AccessLocked<pst::CallList> extern_args
	) {
		std::vector<pst::AccessLocked<pst::CallArgument>> args{ extern_args.unlock(ctx)->begin(),
			                                                    extern_args.unlock(ctx)->end() };

		if (args.empty()) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				"extern() requires at least one argument specifying the ABI",
				extern_args.unlock(ctx)->getStablePosition()
			));
			return query::Failed();
		}

		UNPACK_QRESULT(auto first_arg =, getStrFromCallArg(ctx, args[0]));

		if (first_arg == base::StrID("C")) {
			CAbi result;

			if (args.size() > 2) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Too many arguments for C ABI in extern()",
					extern_args.unlock(ctx)->getStablePosition()
				));
				return query::Failed();
			}

			if (isFunctionLike(kind(sym))) {
				UNPACK_QRESULT_CREF(auto& declaration =, ctx.query<QueryDeclOfFun>(sym));
				// We perform a check here, because unit types are removed from LIR.
				UNPACK_QRESULT(auto _ =, checkCABIParamTypes(ctx, declaration));


				if_opt_some(getAttribute<attributes::CFFIVariadicAfter>(sym), variadic) {
					UNPACK_QRESULT(
						auto fixed_params =,
						validateVariadicAfter(ctx, declaration, variadic->fixed_params)
					);
					result.variadic_after = fixed_params;
				}
			}

			if (args.size() == 2) {  // `extern("C" "mylib")` case
				UNPACK_QRESULT(auto lib_str_lit =, getStrFromCallArg(ctx, args[1]));
				result.library = lib_str_lit;
			}

			return result;
		} else if (first_arg == base::StrID("DVM")) {
			if (args.size() == 1)  // `extern("DVM")` case
				return DVMAbi{};
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				"Too many arguments for DVM ABI in extern()",
				extern_args.unlock(ctx)->getStablePosition()
			));
			return query::Failed();
		} else {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				"Unsupported ABI specified in extern()", extern_args.unlock(ctx)->getStablePosition()
			));
			return query::Failed();
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolABI, QuerySymbolABI_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto sym_ref = getSymRef(key);

			// Builtin functions are implemented in C/C++ and use the C ABI.
			variant_match(sym_ref->other) {
				variant_case_novalue(defgen::BuiltinOperator) { return CAbi{}; }
				variant_default {}
			}

			// @TODO: #895 fix it when we add script based package targets
			if (name(key) == "main" && isGlobalFun(key)) {
				// main is not mangled
				return CAbi{};
			}

			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(key);
			for (auto specifier: *specifiers) {
				if (specifier.unlock(ctx)->getSpecifier().unlock(ctx)->unwrap()
				    == pst::Keyword::Extern) {
					match_optional(specifier.unlock(ctx)->getArgs()) {
						opt_some(args) { return getSymbolABI(ctx, key, args); }
						opt_none {
							ctx.logInt(makeBox<dia_int::PlaceholderError>(
								"extern symbol requires ABI specification passed as an argument",
								specifier.unlock(ctx)->getStablePosition()
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
