#include "query_hout_of_expr.hpp"

#include "ctv/ctv.hpp"
#include "ctv/numeric_value.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios_private/expressions/builtin_operations.hpp>
#include <helios_private/expressions/chain_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <typesystem/higher/queries.hpp>

#include "base/str/str_utils.hpp"
#include "base/str/string_id.hpp"
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include "diagnostic/source_position.hpp"
#include "lang_definitions/key_spec_op.hpp"
#include "query_framework/context.hpp"
#include <query_framework/query_impl.hpp>

#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <variant>

#define DEBUG(CONTENT) std::cout << "[QUERY HOUT OF EXPR]: " << CONTENT << '\n';

#define NDEBUG(CONTENT) std::cout << "[NUMERIC DEDUCTION]: " << CONTENT << '\n';

namespace compiler::helios::code {
	namespace {

		/**
		 * This is an effective implementation of QueryHoutOfExpr.
		 * QueryHoutOfExpr is mostly a wrapper for future cache.
		 * @note This is a private function of this file.
		 */
		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		);

		void getVariantSubExprsInPlace(
			query::Context&                                   ctx,
			pst::AccessLocked<pst::ExprElement>               expr,
			std::vector<pst::AccessLocked<pst::ExprElement>>& sub_exprs_append
		) {
			if (auto bin_op_opt = expr.unlock(ctx).dynamicCast<pst::expr::BinaryOperator>()) {
				auto bin_op = bin_op_opt.value();
				if (bin_op->getOperator().str() == "|") {
					getVariantSubExprsInPlace(ctx, bin_op->getLeftOperand(), sub_exprs_append);
					getVariantSubExprsInPlace(ctx, bin_op->getRightOperand(), sub_exprs_append);
				}
			} else {
				sub_exprs_append.emplace_back(expr);
			}
		}

		/**
		 * @brief Extracts sub expressions from a variant operator.
		 * This flattens PST `a | b | c` expression (only if there are no parenthesis).
		 */
		std::vector<pst::AccessLocked<pst::ExprElement>> getVariantSubExprs(
			query::Context& ctx, pst::AccessLocked<pst::expr::BinaryOperator> expr
		) {
			CORE_ASSERT(expr.unlock(ctx)->getOperator().str() == "|", "Not a variant operator");
			std::vector<pst::AccessLocked<pst::ExprElement>> sub_exprs;
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getLeftOperand(), sub_exprs);
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getRightOperand(), sub_exprs);
			return sub_exprs;
		}

		/// NUMERIC LITERAL PARSING ///
		template<typename TargetType, typename SourceType>
		bool fitsIn(SourceType value) {
			if constexpr (std::is_signed_v<TargetType> == std::is_signed_v<SourceType>) {
				return value >= static_cast<SourceType>(std::numeric_limits<TargetType>::min())
				    && value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
			} else if constexpr (std::is_unsigned_v<SourceType> && std::is_unsigned_v<TargetType>) {
				return value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
			} else {
				return value >= 0
				    && static_cast<SourceType>(value) <= std::numeric_limits<TargetType>::max();
			}
		}

		template<typename TargetInt>
		base::Optional<numeric_value::NumericValue> parseSignedInteger(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Parse signed int");
			// TODOP: i128 potentially?
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: signed integer"
					)
				);
				return {};
			}

			if (!fitsIn<TargetInt>(parsed_value)) {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Literal doesn't fit in the declared signed integer type"
					)
				);
				return {};
			}

			return numeric_value::NumericValue(static_cast<TargetInt>(parsed_value));
		}

		template<typename TargetUInt>
		base::Optional<numeric_value::NumericValue> parseUnsignedInteger(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Parse unsigned int");
			// TODOP: u128 potentially?
			u64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: unsigned integer"
					)
				);
				return {};
			}
			if (!fitsIn<TargetUInt>(parsed_value)) {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Literal doesn't fit in the declared unsigned integer type"
					)
				);
				return {};
			}
			return numeric_value::NumericValue(static_cast<TargetUInt>(parsed_value));
		}

		template<typename TargetFloat>
		base::Optional<numeric_value::NumericValue> parseFloat(
			std::string_view value, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Parse float");
			f128 parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);

			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: floating point"
					)
				);
				return {};
			}
			return numeric_value::NumericValue(static_cast<TargetFloat>(parsed_value));
		}

		base::Optional<numeric_value::NumericValue> deduceIntegerType(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Deduce integer type");
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);
			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				NDEBUG("Deduce integer type from_chars error");
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: deduced integer type"
					)
				);
				return {};
			}

			// TODOP: issue, add support for i8 and i128 types
			numeric_value::NumericValue numeric_result;
			if (parsed_value <= std::numeric_limits<i16>::max()) {
				NDEBUG("i16");
				numeric_result = numeric_value::NumericValue{ static_cast<i16>(parsed_value) };
			} else if (parsed_value <= std::numeric_limits<i32>::max()) {
				NDEBUG("i32");
				numeric_result = numeric_value::NumericValue{ static_cast<i32>(parsed_value) };
			} else if (parsed_value <= std::numeric_limits<i64>::max()) {
				NDEBUG("i64");
				numeric_result = numeric_value::NumericValue{ static_cast<i64>(parsed_value) };
			} else {
				NDEBUG("Deduce integer type literal overflow");
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Integer literal overflow"
					)
				);
				return {};
			}

			// TODOP: Add issue number
			// @TODO: For now, until the cast instuction are added we cast all the deduced types to
			// i64 to avoid adding a type specifier to  every numeric literal in the tests.
			return numeric_value::NumericValue{ std::visit(
				[&](auto&& val) { return static_cast<i64>(val); }, numeric_result.getStorage()
			) };
		}

		base::Optional<numeric_value::NumericValue> deduceFloatType(
			std::string_view value, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Deduce float type");
			f128 parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: deduced floating point type"
					)
				);
				return {};
			}

			// TODOP: Issue, add support for f16.
			if (static_cast<f128>(static_cast<f32>(parsed_value)) == parsed_value)
				return numeric_value::NumericValue{ static_cast<f32>(parsed_value) };
			else if (static_cast<f128>(static_cast<f64>(parsed_value)) == parsed_value)
				return numeric_value::NumericValue{ static_cast<f64>(parsed_value) };
			return numeric_value::NumericValue{ parsed_value };  // Full precision needed
		}

		/**
		 * Visitor that implements logic of creation of HOUT expressions from PST expressions.
		 */
		struct PstExprToHoutExprVisitor final: public pst::expr::PstExprVisitorPanicky {
			explicit PstExprToHoutExprVisitor(query::Context& ctx): ctx(ctx) {}

			query::Context& ctx;

			/**
			 * The "output" of the visitor.
			 */
			base::Optional<base::Box<Expr>> node;

			void visitUnitExpr(pst::Access<pst::expr::UnitExpr>) override {
				DEBUG("Visit unit");
				node = makeBox<LiteralUnitExpr>(ctx);
			}

			void visitExprValue(pst::Access<pst::expr::ExprValue> stmt) override {
				DEBUG("Visit expr value");
				DEBUG("Called for:");
				stmt->dprint(std::cout);
				std::cout << "\n===========================\n";


				auto value = stmt->getValue().value.strView();
				auto type_specifier_strid
					= stmt->getValue().type_specifier.copyValueOr(base::StrID(""));
				auto type_specifier
					= lang_def::strAsNumericLiteralTypeSpecifier(type_specifier_strid);
				auto position = stmt->getSourcePosition();

				int base = 10;
				if (value.starts_with("0b") || value.starts_with("0B")) {
					base = 2;
					value.remove_prefix(2);
				} else if (value.starts_with("0o") || value.starts_with("0O")) {
					base = 8;
					value.remove_prefix(2);
				} else if (value.starts_with("0x") || value.starts_with("0X")) {
					base = 16;
					value.remove_prefix(2);
				}

				base::Optional<numeric_value::NumericValue> parsed_numeric_value;
				switch (type_specifier) {
				case lang_def::NumericLiteralTypeSpecifier::NotATypeSpecifier: {
					bool is_float        = value.find_first_of(".eE") != std::string_view::npos;
					parsed_numeric_value = is_float ? deduceFloatType(value, position, ctx)
					                                : deduceIntegerType(value, base, position, ctx);
					break;
				}
				case lang_def::NumericLiteralTypeSpecifier::i16:
					parsed_numeric_value = parseSignedInteger<i16>(value, base, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::i32:
					parsed_numeric_value = parseSignedInteger<i32>(value, base, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::i64:
					parsed_numeric_value = parseSignedInteger<i64>(value, base, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::u16:
					parsed_numeric_value = parseUnsignedInteger<u16>(value, base, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::u32:
					parsed_numeric_value = parseUnsignedInteger<u32>(value, base, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::u64:
					parsed_numeric_value = parseUnsignedInteger<u64>(value, base, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::f32:
					parsed_numeric_value = parseFloat<f32>(value, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::f64:
					parsed_numeric_value = parseFloat<f64>(value, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::f128:
					parsed_numeric_value = parseFloat<f128>(value, position, ctx);
					break;
				case lang_def::NumericLiteralTypeSpecifier::i8:
				case lang_def::NumericLiteralTypeSpecifier::u8:
				case lang_def::NumericLiteralTypeSpecifier::f16:
				case lang_def::NumericLiteralTypeSpecifier::f80:
				case lang_def::NumericLiteralTypeSpecifier::u128:
				case lang_def::NumericLiteralTypeSpecifier::i128:
					throw base::NotYetImplemented(base::strConcat(
						"Unhandled type specifier in hout of expr: ",
						lang_def::numericLiteralTypeSpecifierToStr(type_specifier)
					));
				}

				if (parsed_numeric_value.has_value()) {  // If failed, the error is logged.
					DEBUG("Visit expr value: RETURN GOOD");
					node = makeBox<LiteralNumericExpr>(ctx, parsed_numeric_value.value());
				}
				DEBUG("Visit expr value: RETURN BAD");
				return;
			}

			void visitExprStrValue(pst::Access<pst::expr::ExprStrValue> stmt) override {
				node = makeBox<LiteralStringExpr>(ctx, stmt->getValue());
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> binaryBuiltin(
				lexer::Operator op, Box<Expr> lhs, Box<Expr> rhs
			) {
				DEBUG("Visit binary builtin");
				auto operation = findBinaryBuiltin(op, lhs.ref(), rhs.ref());
				if (operation) {
					return makeBox<BinaryOperatorExpr>(
						ctx, operation.value(), std::move(lhs), std::move(rhs)
					);
				}
				DEBUG("Visit binary builtin: RETURN BAD");
				return {};
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> unaryBuiltin(lexer::Operator op, Box<Expr> expr) {
				auto operation = findUnaryBuiltin(op, expr.ref());

				DEBUG("Visit unary builtin");
				if (operation)
					return makeBox<UnaryOperatorExpr>(operation.value(), std::move(expr));
				else
					return {};
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				DEBUG("Visit binary op");
				// handle variants:
				if (stmt->getOperator().str() == "|") {
					// @todo HOUT 2.0:
					// Here we assume that "|" always produces a variant (likely valid).
					// If it does not, and "|" will remain a binary operator,
					// we will have to do something with it.
					// (likely if-out if all sub expressions are meta or non-meta, throw otherwise,
					// (require parentheses))

					auto sub_exprs = getVariantSubExprs(ctx, stmt);
					// @todo HOUT 2.0:
					// validate that all sub types are meta

					std::vector<Box<Expr>> all_subtypes;

					for (auto sub_expr: sub_exprs) {
						auto sub_expr_hout = fromPST(ctx, sub_expr);
						if (sub_expr_hout.hasError()) {
							// Error has occurred.
							return;
						}
						all_subtypes.emplace_back(std::move(sub_expr_hout).value());
					}
					node = makeBox<VariantTypeConstructorExpr>(ctx, std::move(all_subtypes));
					return;
				}

				auto lhs_res = fromPST(ctx, stmt->getLeftOperand());
				auto rhs_res = fromPST(ctx, stmt->getRightOperand());

				// @todo: make failure more explicit...
				if (lhs_res.hasError() or rhs_res.hasError()) return;  // failed

				auto lhs = std::move(lhs_res).value();
				auto rhs = std::move(rhs_res).value();

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				auto builtin = binaryBuiltin(stmt->getOperator(), std::move(lhs), std::move(rhs));
				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
							stmt->getSourcePosition(), "No builtin operator found"
						)
					);
					// failed
				}
			}

			void visitChainExpr(pst::Access<pst::expr::ChainExpr> chain_expr) override {
				DEBUG("Visit chain expr");
				auto result = fromChainExpr(ctx, chain_expr);
				if (result.hasError()) {
					// Error has occurred.
					return;
				}
				node = std::move(result.value());
			}

			void visitRoundExpr(pst::Access<pst::expr::RoundExpr> stmt) override {
				DEBUG("Visit round expr");
				PstExprToHoutExprVisitor vis(ctx);
				stmt->getInner().unlock(ctx)->acceptExprVisitor(vis);
				if (vis.node) node = makeBox<ParenthesisExpr>(ctx, std::move(*vis.node));
			}

			void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> stmt) override {
				DEBUG("Visit identifier literal");
				// note: this is a mock, it should be unified with ChainExpr
				auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

				const auto& sym_list = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
					stmt->getName().position, ctx, stmt->getName().value
				);
				if (!sym_list) {
					// failed
					return;
				}

				node = makeBox<IdentifierExpr>(ctx, sym_list.value().back());
			}

			void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> stmt) override {
				DEBUG("Visit keyword literal");
				using enum tsh::IntegralAbstractType::Signedness;
				switch (stmt->getKeyword()) {
				// true, false:
				case pst::Keyword::True:
					node = makeBox<LiteralBoolExpr>(ctx, true);
					break;
				case pst::Keyword::False:
					node = makeBox<LiteralBoolExpr>(ctx, false);
					break;


				// types:
				case pst::Keyword::Bool:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryBoolType>({}));
					break;

				case pst::Keyword::Char:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryCharType>({}));
					break;

				case pst::Keyword::Str:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryStringType>({}));
					break;

					// @todo: add meta keyword and type

				case pst::Keyword::i128:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 128, Signed })
					);
					break;
				case pst::Keyword::i64:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 64, Signed })
					);
					break;
				case pst::Keyword::i32:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 32, Signed })
					);
					break;
				case pst::Keyword::i16:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 16, Signed })
					);
					break;
				case pst::Keyword::i8:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 8, Signed })
					);
					break;

				case pst::Keyword::u128:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 128, Unsigned })
					);
					break;
				case pst::Keyword::u64:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 64, Unsigned })
					);
					break;
				case pst::Keyword::u32:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 32, Unsigned })
					);
					break;
				case pst::Keyword::u16:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 16, Unsigned })
					);
					break;
				case pst::Keyword::u8:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 8, Unsigned })
					);
					break;

				case pst::Keyword::f80:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(80));
					break;
				case pst::Keyword::f128:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(128));
					break;
				case pst::Keyword::f64:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(64));
					break;
				case pst::Keyword::f32:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(32));
					break;
				case pst::Keyword::f16:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(16));
					break;


				default:
					CORE_PANIC(
						"Keyword not yet handled (or bad keyword) by PstExprToHoutExprVisitor"
					);
				}
			}

			void visitComma(pst::Access<pst::expr::Comma> stmt) override {
				std::vector<Box<Expr>> expressions;
				for (auto ex: stmt->getExpressions()) {
					auto res = fromPST(ctx, ex);
					if (res.hasError()) {
						// Error has occurred.
						return;
					}
					expressions.emplace_back(std::move(res).value());
				}

				node = makeBox<TupleTypeConstructorExpr>(ctx, std::move(expressions));
			}

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator>) override {
				// note: here we will have to compile things like `a++`, `a--`, `T?`.
				throw base::NotYetImplemented(
					"Suffix operators are not yet implemented in HOUT, since there are any for now"
				);
			}

			void visitPrefixOperator(pst::Access<pst::expr::PrefixOperator> stmt) override {
				// @NOTE: This is a mockup
				auto inner = fromPST(ctx, stmt->getExpr());
				if (inner.hasError()) return;  // failed

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				auto builtin = unaryBuiltin(stmt->getOperator(), std::move(inner.value()));

				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
							stmt->getSourcePosition(), "No builtin operator found"
						)
					);
					// failed
				}
			}

			void visitTernary(pst::Access<pst::expr::Ternary> stmt) override {
				auto condition_res = fromPST(ctx, stmt->getCondition());
				auto if_true_res   = fromPST(ctx, stmt->getIfTrue());
				auto if_false_res  = fromPST(ctx, stmt->getIfFalse());

				if (condition_res.hasError() or if_true_res.hasError() or if_false_res.hasError())
					return;

				auto condition = std::move(condition_res).value();
				auto if_true   = std::move(if_true_res).value();
				auto if_false  = std::move(if_false_res).value();

				node = makeBox<TernaryOperatorExpr>(
					ctx, std::move(condition), std::move(if_true), std::move(if_false)
				);
			}

			void visitComparisonChain(pst::Access<pst::expr::ComparisonChain> stmt) override {
				using namespace ::std::views;

				const auto& pst_operators  = stmt->getOperators();
				size_t      operator_count = std::ranges::size(pst_operators);
				size_t      expr_count     = operator_count + 1;

				std::vector<Box<Expr>> result_exprs;
				result_exprs.reserve(expr_count);
				for (size_t i = 0; i < expr_count; ++i) {
					auto result = fromPST(ctx, stmt->getSubExpr(i));
					if (result.hasError())
						return;
					else
						result_exprs.push_back(std::move(result.value()));
				}

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:


				std::vector<BuiltinBinary> operators;
				operators.reserve(operator_count);
				for (size_t i = 0; i < operator_count; ++i) {
					match_optional(findBinaryBuiltin(
						pst_operators.at(i), result_exprs.at(i).ref(), result_exprs.at(i + 1).ref()
					)) {
						opt_some(op) { operators.push_back(op); }
						opt_none {
							ctx.log(makeBox<
									dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
								stmt->getSourcePosition(), "No builtin operator found"
							));
							return;
						}
					}
				}

				node = makeBox<ChainComparisonExpr>(
					ctx, std::move(result_exprs), std::move(operators)
				);
			}
		};

		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		) {
			// std::cerr << "\nExpr: \n";
			// root->debugPrint(std::cerr);
			// std::cerr << '\n'

			PstExprToHoutExprVisitor visitor(ctx);
			element.unlock(ctx)->acceptExprVisitor(visitor);

			if_opt_some(visitor.node, expr) return std::move(expr);
			DEBUG("VISITOR FAILED");
			return query::QError(errors::Failed());
		}
	}
}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// Note: we might actually accept nulls in such queries, and just return failed
			// Something to think about as part of #412
			CORE_ASSERT(
				key.element.unlockOpt(ctx).has_value(), "Nullptr provided to QueryHoutOfExpr"
			);

			DEBUG("Called top level query for:");
			key.element.unlock(ctx)->debugPrint(std::cout);
			std::cout << '\n' << "======================" << '\n';

			// @TODO static assert this is top-expr
			return code::fromPST(ctx, key.element);
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)
}
