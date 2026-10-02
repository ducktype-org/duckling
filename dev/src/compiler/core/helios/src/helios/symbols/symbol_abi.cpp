

#include "symbol_abi.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/call_argument.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/specifier_block.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/errors/extern_c_class_empty.hpp>
#include <helios/errors/field_not_c_compatible.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/attributes.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <tsl/c_abi_converter.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <diagnostic/placeholder.hpp>
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
				ctx.logInt(makeBox<dia::PlaceholderError>(
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
		 * @brief Whether a type is valid as variadic parameter type in
		 * `@cffi_variadic_fixed_params` declaration.
		 * @return Empty value if valid, and the value that should be declared instead if invalid.
		 */
		std::expected<void, tsh::AbstractType> validateVariadicArgType(
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
		query::QResult<u64> validateVariadicWithNFixed(
			query::Context& ctx, const HOUTFunctionDeclaration& decl, u64 fixed_params
		) {
			if (fixed_params == 0) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Variadic function declaration should have at least one fixed parameter.",
					decl.origin
				));
				return query::Failed();
			}

			if (fixed_params >= decl.parameters.size()) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
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
					ctx.logInt(makeBox<dia::PlaceholderError>(
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
	}

	QuerySymbolABI_Result getSymbolABI(
		query::Context& ctx, SymID sym, pst::AccessLocked<pst::CallList> extern_args
	) {
		std::vector<pst::AccessLocked<pst::CallArgument>> args{ extern_args.unlock(ctx)->begin(),
			                                                    extern_args.unlock(ctx)->end() };

		if (args.empty()) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				"extern() requires at least one argument specifying the ABI",
				extern_args.unlock(ctx)->getStablePosition()
			));
			return query::Failed();
		}

		UNPACK_QRESULT(auto first_arg =, getStrFromCallArg(ctx, args[0]));

		if (first_arg == base::StrID("C")) {
			CAbi result;

			if (args.size() > 2) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Too many arguments for C ABI in extern()",
					extern_args.unlock(ctx)->getStablePosition()
				));
				return query::Failed();
			}

			if (isFunctionLike(kind(sym))) {
				if_opt_some(getAttribute<attributes::CFFIVariadicFunction>(sym), variadic) {
					result.fixed_params = variadic->fixed_params;
				}
			}

			// Like `@cffi_variadic_fixed_params`, `@c_symbol_name` only means something for the C
			// ABI: on a default-ABI or `extern("DVM")` symbol it is ignored.
			if_opt_some(getAttribute<attributes::CSymbolName>(sym), link_name) {
				result.symbol_name = link_name->name;
			}

			if (args.size() == 2) {  // `extern("C" "mylib")` case
				UNPACK_QRESULT(auto lib_str_lit =, getStrFromCallArg(ctx, args[1]));
				result.library = lib_str_lit;
			}

			return result;
		} else if (first_arg == base::StrID("DVM")) {
			if (args.size() == 1)  // `extern("DVM")` case
				return DVMAbi{};
			ctx.logInt(makeBox<dia::PlaceholderError>(
				"Too many arguments for DVM ABI in extern()",
				extern_args.unlock(ctx)->getStablePosition()
			));
			return query::Failed();
		} else {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				"Unsupported ABI specified in extern()", extern_args.unlock(ctx)->getStablePosition()
			));
			return query::Failed();
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolABI, QuerySymbolABI_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO: #895 fix it when we add script based package targets
			if (isGlobalMain(key)) return MainAbi{};

			auto specifiers = ctx.query<QuerySpecifiersOfSymbol>(key);
			for (auto specifier: *specifiers) {
				if (specifier.unlock(ctx)->getSpecifier().unlock(ctx)->unwrap()
				    == pst::Keyword::Extern) {
					match_optional(specifier.unlock(ctx)->getArgs()) {
						opt_some(args) { return getSymbolABI(ctx, key, args); }
						opt_none {
							ctx.logInt(makeBox<dia::PlaceholderError>(
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

	query::QResult<CRef<SymbolABI>> ABIWrapper::withValidation(
		query::Context&                                                           ctx,
		std::variant<CRef<HOUTFunctionDeclaration>, CRef<tsh::ClassAbstractType>> val
	) const {
		v_if_matches(value, CAbi, c_abi) {
			variant_match(val) {
				variant_case(CRef<HOUTFunctionDeclaration>, decl_ref) {
					const HOUTFunctionDeclaration& decl = *decl_ref;

					auto log_from_cabi = [&](const tsl::CAbiConversionResult&    result,
					                         std::string_view                    elem,
					                         base::Optional<dia::StablePosition> pos) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							base::strConcat(
								"CABI ", elem, " is invalid. Reason: `", result.error(), "`."
							),
							pos
						));
					};

					if_opt_some(c_abi->fixed_params, fixed) {
						UNPACK_QRESULT(auto _ =, validateVariadicWithNFixed(ctx, decl, fixed));
					}

					for (auto& param: decl.parameters) {
						UNPACK_QRESULT_CREF(
							auto& result =, ctx.query<tsl::QueryCAbiTypeOf>(param.type)
						);
						if (not result.has_value()) {
							log_from_cabi(result, "function parameter", param.origin);
							return query::Failed();
						}
					}

					const bool valueless_return
						= decl.return_type.getRefKind() == tsh::ReferenceKind::Direct
					  and (decl.return_type.getType().getKind() == tsh::Kind::Unit
					       or decl.return_type.getType().getKind() == tsh::Kind::Void);

					if (not valueless_return) {
						UNPACK_QRESULT_CREF(
							auto& result =, ctx.query<tsl::QueryCAbiTypeOf>(decl.return_type)
						);
						if (not result.has_value()) {
							log_from_cabi(result, "function return type", decl.origin);
							return query::Failed();
						}
					}
				}
				variant_case(CRef<tsh::ClassAbstractType>, class_ref) {
					const tsh::ClassAbstractType& class_type = *class_ref;

					const dia::StablePosition class_position
						= maybeSymbolPst(class_type.getSymbol())
					          .value()
					          .unlock(ctx)
					          ->getStablePosition();

					bool any_field  = false;
					bool any_failed = false;
					for (const auto& element: class_type.getInterface(ctx)->getElements()) {
						if (not element.isField()) continue;
						any_field = true;

						UNPACK_QRESULT_CREF(
							auto& field_result =,
							ctx.query<tsl::QueryCAbiTypeOf>(element.getType(ctx))
						);
						if (field_result.has_value()) continue;
						any_failed = true;

						const auto field_pst = maybeSymbolPst(element.getSymbol());
						ctx.logInt(makeBox<FieldNotCCompatibleError>(
							ctx,
							field_pst.has_value()
								? field_pst.value().unlock(ctx)->getStablePosition()
								: class_position,
							std::string(name(element.getSymbol()).strView()),
							element.getType(ctx),
							field_result.error()
						));
					}

					// C has no zero-sized structs, so a class without fields has no C layout.
					if (not any_field) {
						ctx.logInt(makeBox<ExternCClassEmptyError>(
							class_position, std::string(name(class_type.getSymbol()).strView())
						));
						return query::Failed();
					}

					if (any_failed) return query::Failed();
				}
			}
		}

		return CRef<SymbolABI>(&value);
	}
}
