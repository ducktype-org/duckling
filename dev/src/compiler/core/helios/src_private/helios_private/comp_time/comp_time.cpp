#include "comp_time.hpp"

#include <ctv/ctv.hpp>
#include <ctv/numeric_value.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/comp_time/vm_evaluator.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <tsl/queries.hpp>
#include <tsl/type_layout.hpp>

#include <base/str/str_utils.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <cmath>
#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>
#include <unordered_set>

namespace compiler::helios {
	using namespace ctv;

	// Integer exponentiation with deterministic two's-complement wraparound.
	// std::pow routes through double and the out-of-range float->int cast is UB:
	// x86 wraps, arm64 saturates. Comptime relies on wrapping (e.g. `2 ** 31 - 1`),
	// so compute it via modular unsigned arithmetic instead.
	template<std::integral IntT>
	IntT comptimeIntPow(IntT base, IntT exp) {
		// A negative exponent truncates toward zero: only |base| == 1 survives.
		if constexpr (std::is_signed_v<IntT>) {
			if (exp < 0) {
				if (base != 1 && base != -1) return 0;
				return exp % 2 == 0 ? IntT{ 1 } : base;
			}
		}
		// Square-and-multiply mod 2^64. Signed values sign-extend, which preserves
		// congruence mod 2^N, so the final truncation is the exact wrapped result.
		u64  result = 1;
		auto b      = static_cast<u64>(base);
		for (auto e = static_cast<u64>(exp); e != 0; e /= 2) {
			if (e % 2 == 1) result *= b;
			b *= b;
		}
		return static_cast<IntT>(result);
	}

	struct IMPLEMENT_QUERY(QueryEvaluateHOUTExpression, CompTimeEvalResult) {
		/**
		 * @brief Error indicating that an expression was to complex for a simple tree evaluation.
		 */
		struct CouldNotShortPath {};

		using TreeEvalValue  = std::variant<CompileTimeValue, CouldNotShortPath>;
		using TreeEvalResult = query::QResult<TreeEvalValue>;

		/**
		 * @brief A HOUT visitor for compile-time expression evaluation.
		 */
		struct TreeEvalVisitor final: public code::HoutExprVisitor {
			query::Context& ctx;
			TreeEvalResult  result;

			TreeEvalVisitor(query::Context& ctx): ctx(ctx) {}

			TreeEvalResult evaluateSubExpr(CRef<code::Expr> expr) {
				expr->acceptVisitor(*this);
				return std::move(result);
			}

			void visitLiteralUnitExpr(const code::LiteralUnitExpr&) final {
				result = CompileTimeValue{ CompileTimeValue::UnitCTV{} };
			}

			void visitLiteralNumericExpr(const code::LiteralNumericExpr& expr) final {
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralBoolExpr(const code::LiteralBoolExpr& expr) final {
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralCharExpr(const code::LiteralCharExpr& expr) final {
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralStringExpr(const code::LiteralStringExpr& expr) final {
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) final {
				result = CompileTimeValue{ expr.value_type };
			}

			/**
			 * @brief Short path for the `size_of` / `alignment_of` builtins: evaluate the (single)
			 * type argument and read its layout directly, instead of falling back to VM evaluation.
			 * @return The evaluated result (or a `Failed` error) when the call matched a short-path
			 * builtin, or `std::nullopt` when the call should fall back to VM evaluation.
			 */
			std::optional<TreeEvalResult> tryShortPathCall(const code::CallExpr& expr) {
				const auto callee_sym = getIdentifierExprSymID(expr.callee.ref());
				if (!callee_sym.has_value()) return std::nullopt;

				const auto builtin = isBuiltin(callee_sym.value());
				if (!builtin.has_value()
				    || (builtin.value() != BuiltinKind::SizeOf
				        && builtin.value() != BuiltinKind::AlignmentOf))
					return std::nullopt;

				CORE_ASSERT(
					expr.arguments.size() == 1, "size_of / alignment_of expect exactly one argument"
				);

				auto arg_res = evalHoutExpr(ctx, expr.arguments.at(0).ref());
				if (arg_res.hasFailed()) return TreeEvalResult{ query::Failed() };

				const auto  type   = arg_res.valueOrThrow().get<tsh::SymbolType<>>().value();
				const auto& layout = ctx.query<tsl::QuerySymbolTypeLayout>(type)->valueOrThrow();

				const i64 value
					= (builtin.value() == BuiltinKind::SizeOf)
				        ? base::safeIntConv<i64>(base::bits2bytesRoundUp(layout.getSize()).asInt())
				        : base::safeIntConv<i64>(layout.getAlignment().asInt());

				return TreeEvalResult{ CompileTimeValue{ NumericValue{ value } } };
			}

			void visitCallExpr(const code::CallExpr& expr) final {
				if (auto short_path = tryShortPathCall(expr)) {
					result = std::move(*short_path);
					return;
				}

				result = CouldNotShortPath{};
			}

			void visitAccessExpr(const code::AccessExpr& expr) final {
				// @TODO: #1922 Implement that.
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Evaluating access expressions at compile time.", expr.origin.getStablePosition()
				));
				result = query::Failed();
			}

			/**
			 * @brief Recursively nests a new static array dimension as the innermost element type.
			 *
			 * If not for that sinking `int[2][3]` would be interpreted as a array with three
			 * elements, each of them being a 2 element array. After this function runs, the type is
			 * correctly interpreted as a 2 element array, with each of its element being a 3
			 * element array.
			 *
			 * @return tsh::SymbolType<> A new type with the correctly nested dimension.
			 */
			tsh::SymbolType<> sinkStaticArrayDimension(
				query::Context& ctx, tsh::SymbolType<> base, usize size
			) {
				if (base.getType().getKind() == tsh::Kind::StaticArray) {
					auto static_arr = base.getType().as<tsh::StaticArrayAbstractType>();
					auto inner_type
						= sinkStaticArrayDimension(ctx, static_arr.getElementType(), size);

					return tsh::SymbolType<>{
						ctx.query<tsh::QueryStaticArrayType>({ inner_type, static_arr.getSize() }),
						base.getRefKind(),
						base.getMutability()
					};
				}
				return tsh::SymbolType<>{ ctx.query<tsh::QueryStaticArrayType>({ base, size }),
					                      base.getRefKind(),
					                      base.getMutability() };
			}

			/**
			 * @brief Evaluates indexing operations performed on meta types
			 *
			 * This includes:
			 * - For type templates: specializing a TypeTemplate with a type when the index provided
			 * is a meta type. Currently only implemented for the builtin List type.
			 * - For static arrays: constructs a static array type with a fixed size `Int[10]` when
			 * the index is a integral constant.
			 *
			 * @param base_type The meta-type being indexed.
			 * @param index_ctv The evaluated CTV used as the index.
			 * @return A QResult containing the newly constructed SymbolType wrapped in a
			 * CTV.
			 */
			auto evaluateTypeIndexing(
				const tsh::SymbolType<>& base_type, const ctv::CompileTimeValue& index_ctv
			) -> query::QResult<ctv::CompileTimeValue> {
				auto base_abs = base_type.getType();

				if (base_abs.getKind() == tsh::Kind::TypeTemplate) {
					// If base is a TypeTemplate type, we expect a meta in the index expression. It
					// instantiates the type template.
					auto template_type = base_abs.as<tsh::TypeTemplateAbstractType>();
					// We just call `.value()` here since the type correctness should be verified
					// earlier.
					auto elem_type = index_ctv.get<tsh::SymbolType<>>().value();

					// Instantiate the type template.
					auto instantiated_abs_type = template_type.instantiate(ctx, elem_type);

					return CompileTimeValue{ tsh::SymbolType<>{
						instantiated_abs_type,
						base_type.getRefKind(),
						base_type.getMutability(),
					} };
				} else {
					// If base is meta and not a type template, then the index should be an integral
					// constant. This expression creates a new static array type.
					auto maybe_size = index_ctv.get<NumericValue>().value();
					CORE_ASSERT(
						maybe_size.isIntegral(),
						"Static array type creation with non-integral size. This should be caught "
						"earlier."
					);
					auto maybe_u64_size = maybe_size.coerceTo<u64>();
					// @TODO: #2754 With flexible literals it should work
					if (!maybe_u64_size.has_value()) {
						ctx.logInt(makeBox<dia_int::PlaceholderError>(
							"Static array size must be a non-negative integral value.", ""
						));
						return query::Failed();
					}
					auto size = static_cast<usize>(maybe_u64_size.value());
					return CompileTimeValue{ sinkStaticArrayDimension(ctx, base_type, size) };
				}
			}

			void visitIndexExpr(const code::IndexExpr& expr) final {
				auto base_res = evalHoutExpr(ctx, expr.base.ref());
				if (base_res.hasFailed()) {
					result = query::Failed();
					return;
				}

				const auto& base_ctv = base_res.valueOrThrow();

				// Index expr on meta is evaluated to a static array type or a list type.
				if (auto maybe_type = base_ctv.get<tsh::SymbolType<>>()) {
					auto index_res = evalHoutExpr(ctx, expr.index.ref());
					if (index_res.hasFailed()) {
						result = query::Failed();
						return;
					}

					auto indexing_res = evaluateTypeIndexing(*maybe_type, index_res.valueOrThrow());
					result            = indexing_res.valueOrThrow();
					return;
				}

				// @TODO: #1922 If base is not meta and not a type template, this is a normal index
				// expression. Implement that.
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Evaluating index expressions with non-meta and non-type-template base at "
					"compile time.",
					expr.origin.getStablePosition()
				));
				result = query::Failed();
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
				if (kind(expr.symbol) == SymbolKind::Const) {
					// Constant Evaluation.
					auto const_val_result = ctx.query<QueryConstValueOf>({ expr.symbol });
					result                = const_val_result.valueOrThrow();
				} else if (kind(expr.symbol) == SymbolKind::Class) {
					// Special case for type definitions.
					auto type = ctx.query<QueryTypeFromDefinition>({ expr.symbol });
					result    = type->valueOrThrow();
				} else {
					match_optional(expr.origin.getStablePosition()) {
						opt_some(pos) {
							ctx.logInt(makeBox<dia_int::PlaceholderError>(
								"Expression cannot be evaluated at compile-time.", pos
							));
						}
						opt_none {
							ctx.logInt(makeBox<dia_int::PlaceholderError>(
								"Expression cannot be evaluated at compile-time.",
								base::strConcat(
									"The code is unavailable because the expression is at "
									"least partially compiler generated.",
									"The failure happened for the symbol `",
									name(expr.symbol),
									"`."
								)
							));
						}
					}
					result = query::Failed();
					return;
				}
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
				using enum code::BuiltinBinary;
				auto lhs_result = evalHoutExpr(ctx, expr.lhs.ref());
				if (lhs_result.hasFailed()) {
					result = query::Failed();
					return;
				}

				auto rhs_result = evalHoutExpr(ctx, expr.rhs.ref());
				if (rhs_result.hasFailed()) {
					result = query::Failed();
					return;
				}

				const auto& lhs_ctv = lhs_result.valueOrThrow();
				const auto& rhs_ctv = rhs_result.valueOrThrow();

				result = std::visit(
					[&](auto&& lhs, auto&& rhs) -> TreeEvalResult {
						using LhsT = std::decay_t<decltype(lhs)>;
						using RhsT = std::decay_t<decltype(rhs)>;

						// Binary operation on numeric literals.
						if constexpr (std::is_same_v<LhsT, NumericValue>
					                  && std::is_same_v<RhsT, NumericValue>) {
							return std::visit(
								[&](auto&& lhs_val) -> TreeEvalResult {
									using LhsNumT = std::decay_t<decltype(lhs_val)>;

									// @note: We assume both sides of the binary operation have the
							        // same types. If types differ, they should be casted with the
							        // cast expr beforehand.
									auto maybe_rhs_val = rhs.template get<LhsNumT>();
									if (!maybe_rhs_val.has_value()) {
										CORE_PANIC(base::strConcat(
											"Operands on binary expression evaluated at compile "
											"time are of different type. This should be "
											"prevented by casts.\nLeft side is:",
											lhs.getTypeOfStoredValue(ctx).getType().toString(),
											"\nRight side is: ",
											rhs.getTypeOfStoredValue(ctx).getType().toString()
										));
									}

									LhsNumT rhs_val = maybe_rhs_val.value();

									using ResultT = LhsNumT;
									CompileTimeValue result;
									auto set_bool_result = [&result](const bool bool_result) {
										result = CompileTimeValue{ bool_result };
									};
									auto set_num_result = [&result](const ResultT num_result) {
										result = CompileTimeValue{ NumericValue{ num_result } };
									};

									switch (expr.operation) {
									case IntegerLt:
									case FloatLt:
										set_bool_result(lhs_val < rhs_val);
										break;
									case IntegerLteq:
									case FloatLteq:
										set_bool_result(lhs_val <= rhs_val);
										break;
									case IntegerGt:
									case FloatGt:
										set_bool_result(lhs_val > rhs_val);
										break;
									case IntegerGteq:
									case FloatGteq:
										set_bool_result(lhs_val >= rhs_val);
										break;
									case IntegerEq:
									case FloatEq:
										set_bool_result(lhs_val == rhs_val);
										break;
									case IntegerNeq:
									case FloatNeq:
										set_bool_result(lhs_val != rhs_val);
										break;
									case IntegerAdd:
									case FloatAdd:
										set_num_result(lhs_val + rhs_val);
										break;
									case IntegerSub:
									case FloatSub:
										set_num_result(lhs_val - rhs_val);
										break;
									case IntegerMul:
									case FloatMul:
										set_num_result(lhs_val * rhs_val);
										break;
									case IntegerDiv:
									case FloatDiv:
										if (rhs_val == 0) {
											ctx.logInt(makeBox<dia_int::PlaceholderError>(
												"Division by zero in compile-time expression "
												"evaluation.",
												expr.origin.getStablePosition().value()
											));
											return query::Failed();
										}
										set_num_result(lhs_val / rhs_val);
										break;
									case IntegerMod:
									case FloatMod:
										if (rhs_val == 0) {
											ctx.logInt(makeBox<dia_int::PlaceholderError>(
												"Modulo by zero in compile-time expression "
												"evaluation.",
												expr.origin.getStablePosition().value()
											));
											return query::Failed();
										}
										if constexpr (std::is_integral_v<ResultT>)
											set_num_result(lhs_val % rhs_val);
										else
											set_num_result(std::fmod(lhs_val, rhs_val));
										break;
									case IntegerPow:
									case FloatPow:
										if constexpr (std::is_integral_v<ResultT>)
											set_num_result(comptimeIntPow<ResultT>(lhs_val, rhs_val)
									        );
										else
											set_num_result(
												static_cast<ResultT>(std::pow(lhs_val, rhs_val))
											);
										break;
									default:
										ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
											"Evaluation of this binary operator at compile "
											"time",
											expr.origin.getStablePosition()
										));
										return query::Failed();
									}

									return result;
								},
								lhs.getStorage()
							);
						} else if constexpr (std::is_same_v<LhsT, bool>
					                         && std::is_same_v<RhsT, bool>) {
							using enum code::BuiltinBinary;
							switch (expr.operation) {
							case BooleanAnd:
								return CompileTimeValue{ lhs && rhs };
							case BooleanOr:
								return CompileTimeValue{ lhs || rhs };
							case IntegerEq:
								return CompileTimeValue{ lhs == rhs };
							case IntegerNeq:
								return CompileTimeValue{ lhs != rhs };
							default:
								ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
									"Evaluation of this binary operator at compile "
									"time",
									expr.origin.getStablePosition()
								));
								return query::Failed();
							}
						} else if constexpr (std::is_same_v<LhsT, tsh::SymbolType<>>
					                         && std::is_same_v<RhsT, tsh::SymbolType<>>) {
							using enum code::BuiltinBinary;
							switch (expr.operation) {
							case MetaEq:
								return CompileTimeValue(lhs == rhs);
							case MetaNeq:
								return CompileTimeValue(lhs != rhs);
							default:
								CORE_UNREACHABLE();
							}
						} else {
							// Unsupported type for binary operator.
							return query::Failed();
						}
					},
					lhs_ctv.getStorage(),
					rhs_ctv.getStorage()
				);
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
				auto expr_result = evalHoutExpr(ctx, expr.expr.ref());
				if (expr_result.hasFailed()) {
					result = query::Failed();
					return;
				}

				const auto& ctv = expr_result.valueOrThrow();


				result = std::visit(
					[&](auto&& val) -> TreeEvalResult {
						using T = std::decay_t<decltype(val)>;
						using enum code::BuiltinUnary;

						if constexpr (std::is_same_v<T, NumericValue>) {
							switch (expr.operation) {
							case IntegerNegation:
							case FloatNegation:
								return std::visit(
									[&](auto&& num_val) -> TreeEvalResult {
										using NumT = std::decay_t<decltype(num_val)>;
										if constexpr (std::is_unsigned_v<NumT>)
											return query::Failed();
										else {
											// @note: static cast is needed here. Since cpp
									        // automatically promotes small int types to i32 if any
									        // operation if performed on them.
											return CompileTimeValue{ NumericValue{
												static_cast<NumT>(-num_val) } };
										}
									},
									val.getStorage()
								);

							default:
								ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
									"Evaluation of this unary operator at compile "
									"time",
									expr.origin.getStablePosition()
								));
								return query::Failed();
							}
						} else if constexpr (std::is_same_v<T, bool>) {
							switch (expr.operation) {
							case code::BuiltinUnary::BooleanNot:
								return CompileTimeValue{ not val };
							default:
								return query::Failed();
							}

						} else if constexpr (std::is_same_v<T, tsh::SymbolType<>>) {
							switch (expr.operation) {
							case Ref:
								return ctv::CompileTimeValue{
									val.withReferenceKind(tsh::ReferenceKind::Ref)
								};
							case Box:
								return ctv::CompileTimeValue{
									val.withReferenceKind(tsh::ReferenceKind::Box)
								};
							case Const:
								return ctv::CompileTimeValue{
									val.withMutability(tsh::Mutability::Immutable)
								};
							case Ptr:
								return ctv::CompileTimeValue{ tsh::SymbolType<>::withDefaults(
									ctx.query<tsh::QueryPointerType>({ val })
								) };
							case ManyPtr:
								return ctv::CompileTimeValue{ tsh::SymbolType<>::withDefaults(
									ctx.query<tsh::QueryManyPointerType>({ val })
								) };
							case CPtr:
								return ctv::CompileTimeValue{ tsh::SymbolType<>::withDefaults(
									ctx.query<tsh::QueryCPointerType>({ val })
								) };
							case Slice:
								return ctv::CompileTimeValue{ tsh::SymbolType<>::withDefaults(
									ctx.query<tsh::QuerySliceType>({ val })
								) };
							default:
								ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
									"Evaluation of this unary operator at compile "
									"time",
									expr.origin.getStablePosition()
								));
								return query::Failed();
							}
						} else {
							ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
								"Evaluation of this unary operator at compile "
								"time",
								expr.origin.getStablePosition()
							));
							return query::Failed();
						}
					},
					ctv.getStorage()
				);
			}

			void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) final {
				auto cond_result = evalHoutExpr(ctx, expr.condition.ref());
				if (cond_result.hasFailed()) {
					result = query::Failed();
					return;
				}

				bool        condition_is_true = false;
				const auto& cond_ctv          = cond_result.valueOrThrow();
				match_optional(cond_ctv.get<bool>()) {
					opt_some(value) { condition_is_true = value; }
					opt_none {
						CORE_PANIC(
							"Ternary operator got a non boolean value when evaluating the ternary "
							"operator condition"
						);
					}
				}

				if (condition_is_true) {
					auto sub_result = evalHoutExpr(ctx, expr.if_true.ref());
					if (sub_result.hasFailed()) {
						result = query::Failed();
						return;
					}
					result = sub_result.valueOrThrow();
				} else {
					auto sub_result = evalHoutExpr(ctx, expr.if_false.ref());
					if (sub_result.hasFailed()) {
						result = query::Failed();
						return;
					}
					result = sub_result.valueOrThrow();
				}
			}

			void visitChainComparisonExpr(const code::ChainComparisonExpr& chain_expr) final {
				using namespace std::views;

				auto evaluate_subexpr = [this](const base::Box<code::Expr>& expr) {
					return evalHoutExpr(ctx, expr.ref());
				};

				// Each expression is evaluated lazily, when it becomes useful.
				auto evaluated_comps = chain_expr.comparisons | transform(evaluate_subexpr);

				for (const auto& evaluated_comp: evaluated_comps) {
					if (evaluated_comp.hasFailed()) {
						result = query::Failed();
						return;
					}

					if (not evaluated_comp.valueOrThrow().get<bool>().value()) {
						result = CompileTimeValue{ false };
						return;
					}
				}
				result = CompileTimeValue{ true };
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
				auto sub_result = evalHoutExpr(ctx, expr.inner.ref());
				if (sub_result.hasFailed()) {
					result = query::Failed();
					return;
				}
				result = sub_result.valueOrThrow();
			}

			void visitTupleExpr(const code::TupleExpr& expr) final {
				std::vector<CompileTimeValue> ctv_elements;

				for (auto& sub_expr: expr.elements) {
					const auto ctv_element_result = evalHoutExpr(ctx, sub_expr.ref());
					if (ctv_element_result.hasFailed()) {
						result = query::Failed();
						return;
					}
					ctv_elements.emplace_back(ctv_element_result.valueOrThrow());
				}

				result = CompileTimeValue{ CompileTimeValue::TupleCTV{ std::move(ctv_elements) } };
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) final {
				std::vector<tsh::SymbolType<>> subtypes;

				for (auto& sub_type: expr.subtypes) {
					CORE_ASSERT(
						sub_type->expression_type.getType().getKind() == tsh::Kind::Meta,
						"It's impossible to create a variant of non-type sub-types in HOUT"
					);

					const auto sub_type_ctv = evalHoutExpr(ctx, sub_type.ref());
					if (sub_type_ctv.hasFailed()) {
						result = query::Failed();
						return;
					}

					subtypes.emplace_back(
						sub_type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value()
					);
				}

				result = CompileTimeValue{ tsh::SymbolType<>{
					ctx.query<tsh::QueryVariantType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				} };
			}

			void visitSequenceExpr(const code::SequenceExpr& seq) final {
				auto sub_result = evalHoutExpr(ctx, seq.expressions.back().ref());
				if (sub_result.hasFailed()) {
					result = query::Failed();
					return;
				}
				result = sub_result.valueOrThrow();
			}

			void visitCastExpr(const code::CastExpr& cast) final {
				auto expr_to_cast = evalHoutExpr(ctx, cast.source_expr.ref());
				if (expr_to_cast.hasFailed()) {
					result = query::Failed();
					return;
				}
				const auto& ctv     = expr_to_cast.valueOrThrow();
				const auto& numeric = ctv.get<NumericValue>();
				if (!numeric) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Casts of non-numeric compile-time values are not yet implemented.",
						cast.origin.getStablePosition()
					));
					result = query::Failed();
					return;
				}


				auto maybe_new_numeric = numeric->castTo(cast.target_type.getType());

				if (!maybe_new_numeric.has_value()) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Value cannot be converted to type `",
							cast.target_type.toString(),
							"` at compile-time."
						),
						cast.origin.getStablePosition().value()
					));
					result = query::Failed();
					return;
				}

				result = CompileTimeValue{ maybe_new_numeric.value() };
			}

			void visitMoveExpr(const code::MoveExpr&) final { result = CouldNotShortPath{}; }

			void visitRefOfExpr(const code::RefOfExpr&) final { result = CouldNotShortPath{}; }

			void visitDerefExpr(const code::DerefExpr&) final { result = CouldNotShortPath{}; }

			void visitDefaultValueExpr(const code::DefaultValueExpr& expr) final {
				switch (expr.type.getKind()) {
				case tsh::Kind::Integral:
				case tsh::Kind::Float: {
					// Creates a 0 initialized numeric by default.
					auto numeric_result = ctv::NumericValue::createOfType(expr.type);
					result              = ctv::CompileTimeValue(numeric_result.expect(
                        base::strConcat("Failed to create 0 of type: ", expr.type.toString())
                    ));
					break;
				}
				case tsh::Kind::Bool: {
					result = ctv::CompileTimeValue(false);
					break;
				}
				case tsh::Kind::String: {
					result = ctv::CompileTimeValue(base::StrID(""));
					break;
				}
				default:
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						base::strConcat(
							"Default value evaluation at compile time for type: '",
							expr.type.toString(),
							"'."
						),
						expr.origin.getStablePosition()
					));
					result = query::Failed();
				}
			}

			/**
			 * @brief Recursively lifts a CompileTimeValue representing a type, a tuple of types,
			 * or a unit to a type.
			 * @note Assumes that the CTV can be lifted to a type, because it assumes that
			 * this has been checked beforehand (e.g. by coercions). Panics if this is not the case.
			 */
			static tsh::SymbolType<> liftCTVToTypeRecursively(
				query::Context& ctx, const CompileTimeValue& ctv
			) {
				variant_match(ctv.getStorage()) {
					variant_case(tsh::SymbolType<>, symbol_type) { return symbol_type; }
					variant_case_novalue(CompileTimeValue::UnitCTV) {
						return tsh::SymbolType<>{
							tsh::getUnitType(),
							tsh::ReferenceKind::Direct,
							tsh::Mutability::Mutable,
						};
					}
					variant_case(CompileTimeValue::TupleCTV, tuple) {
						std::vector<tsh::SymbolType<>> element_types;
						element_types.reserve(tuple.getElements().size());
						for (const auto& sub_ctv: tuple.getElements())
							element_types.emplace_back(liftCTVToTypeRecursively(ctx, sub_ctv));
						return tsh::SymbolType<>{
							ctx.query<tsh::QueryTupleType>({ std::move(element_types) }),
							tsh::ReferenceKind::Direct,
							tsh::Mutability::Mutable,
						};
					}
				}
				CORE_UNREACHABLE();
			}

			void visitLiftToTypeExpr(const code::LiftToTypeExpr& lift) final {
				auto ctv_to_lift = evalHoutExpr(ctx, lift.value_expr.ref());
				if (ctv_to_lift.hasFailed()) {
					result = query::Failed();
					return;
				}
				// Panics if the CTV cannot be lifted to a type.
				// This is fine, because we assume that this has been checked beforehand by HOUT.
				result
					= CompileTimeValue(liftCTVToTypeRecursively(ctx, ctv_to_lift.valueOrThrow()));
			}

			void visitReusableExpr(const code::ReusableExpr& reusable) override {
				evaluateSubExpr(reusable.inner.ref());
			}

			void visitListPushExpr(const code::ListPushExpr& expr) final {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Evaluating list push expression at compile time.",
					expr.origin.getStablePosition()
				));
			}

			void visitListPopExpr(const code::ListPopExpr& expr) final {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Evaluating list pop expression at compile time.",
					expr.origin.getStablePosition()
				));
			}
		};

		/**
		 * @brief A POD to encapsulate the results of `prepareLIRForDVM`.
		 * - `func_to_call` - a mangled name of the function we evaluate.
		 * - `functions` 	- a vector of all LIR functions to compile and load to DVM in order to
		 * 					  evaluate `func_to_call`
		 */
		struct LIRBuildResult final {
			/// Mangled name of the function we evaluate.
			std::string func_to_call;
			/// List of LIR functions and other entites needed to evaluate `func_to_call`.
			lir::LIRUnit lir_unit;
		};

		/**
		 * @brief Collects all data needed to evaluate a function with the given `function_sym_id`
		 * (target function). This includes:
		 *- Collection all function dependencies - collecting all functions which are being called
		 * by the target function.
		 * - Retrieving the mangled name of the target function
		 * - Compiling all of the necessary functions to LIR
		 *
		 * This data along with the argument CTVs is passed to `CompTimeDVM`, compiled to bytecode
		 * and the function with the mangled name is called.
		 *
		 * @return A `QResult` with either a `LIRBuildResult` a (mangled_function_name,
		 * all_necessary_lir_functions) or a `query::Failed` if any of the steps on the way failed.
		 */
		static query::QResult<LIRBuildResult> prepareLIRForDVM(
			query::Context& ctx, SymID function_sym_id
		) {
			// Collect all function dependencies for this function. All functions needed in
			// order to evaluate this one.
			auto& all_dependencies = ctx.query<QueryTransitiveUsedSymbols>(function_sym_id)
			                             ->valueOrThrow()
			                             .used_functions;

			auto mangled_name_function_to_call
				= ctx.query<mangler::QueryMangledSymbol>({ .symbol_key = function_sym_id });

			// Temporary hout unit used to lower functions to LIR. `all_dependencies` excludes the
			// target function itself, so we lower it explicitly alongside its transitive callees.
			HOUTUnit hout_unit;
			if (implementsQueryCodeOfFun(function_sym_id))
				hout_unit.functions.emplace_back(
					&ctx.query<QueryCodeOfFun>(function_sym_id)->valueOrThrow()
				);
			for (const SymID& func_id: all_dependencies) {
				if (not implementsQueryCodeOfFun(func_id)) continue;
				auto& hout_func = ctx.query<QueryCodeOfFun>(func_id)->valueOrThrow();
				hout_unit.functions.emplace_back(&hout_func);
			}
			auto lir_unit
				= lir::lowerToLIRUnit(ctx, mir::lowerToMIRUnit(ctx, &hout_unit).valueOrThrow());

			// Note: the assumptions bellow might change,
			// for example when we will add consts to comp time.
			// Bare declarations (no lowerable body) are filtered out above, so we compare against
			// the functions actually lowered, not every collected dependency.
			CORE_ASSERT(
				lir_unit.lir_functions.size() == hout_unit.functions.size(),
				"Number of lir functions should be the same as number of lowered dependencies."
			);
			CORE_ASSERT(
				lir_unit.lir_globals.empty(),
				"LIR global variables are not supported in compile time evaluation."
			);

			return LIRBuildResult{
				.func_to_call = mangled_name_function_to_call.str(),
				.lir_unit     = std::move(lir_unit),
			};
		}

		/**
		 * @brief Evaluates all argument expressions to CompileTimeValues.
		 */
		static query::QResult<std::vector<CompileTimeValue>> evaluateArguments(
			query::Context& ctx, const std::vector<Box<code::Expr>>& args
		) {
			std::vector<CompileTimeValue> ctv_arguments;
			ctv_arguments.reserve(args.size());

			for (const auto& arg_expr: args) {
				UNPACK_QRESULT(auto arg =, evalHoutExpr(ctx, arg_expr.ref()));
				ctv_arguments.push_back(std::move(arg));
			}

			return ctv_arguments;
		}

		/**
		 * @brief Evaluates a HOUT call expression using DVM Eval.
		 * @return The calculated result represented by CompileTimeValue or a Failed error.
		 */
		static CompTimeEvalResult evaluateFunctionWithVm(
			query::Context& ctx, CRef<code::CallExpr> call_expr
		) {
			const auto* callee_ident
				= dynamic_cast<const code::IdentifierExpr*>(call_expr->callee.get());
			if (!callee_ident) return query::Failed();

			const SymID function_sym_id = callee_ident->symbol;

			auto args_result = evaluateArguments(ctx, call_expr->arguments);
			if (args_result.hasFailed()) return query::Failed();
			auto ctv_arguments = std::move(args_result.valueOrThrow());

			auto lir_build_result = prepareLIRForDVM(ctx, function_sym_id);
			if (lir_build_result.hasFailed()) return query::Failed();
			const auto& [func_to_call_name, lir_unit] = lir_build_result.valueOrThrow();


			// Retrieve the functions return type.
			auto callee_abs_type = callee_ident->expression_type.getSymbolType().getType();
			CORE_ASSERT(
				callee_abs_type.getKind() == tsh::Kind::Function,
				"Attempting to call a non_function type during VM compile time evaluation"
			);
			tsh::FunctionAbstractType func_type(callee_abs_type);

			auto vm_eval_result = executeInVm(
				ctx, func_to_call_name, lir_unit, ctv_arguments, func_type.getResultType()
			);

			if (!vm_eval_result) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Compile time evaluation of this function call failed or "
					"returned unsupported result.",
					call_expr->origin.getStablePosition(),
					base::strConcat("Detailed reason: ", vm_eval_result.error().message, "\n")
				));
				return query::Failed();
			}
			return vm_eval_result.value();
		}

		/**
		 * @brief Evaluates a HOUT expression using TreeEval.
		 * @return The calculated result represented by CompileTimeValue, a CouldNotShortPath
		 * error if the expression was too complicated for tree eval or a Failed error.
		 */
		static auto evaluateWithTreeEval(query::Context& ctx, CRef<code::Expr> expr)
			-> TreeEvalResult {
			TreeEvalVisitor visitor(ctx);
			expr->acceptVisitor(visitor);
			return visitor.result;
		}

		/**
		 * @brief Evaluates a HOUT expression using TreeEval or VMEval if the expression is to
		 * complex for tree eval.
		 * @return The calculated result represented by CompileTimeValue or a Failed error.
		 */
		static auto evalHoutExpr(query::Context& ctx, CRef<code::Expr> expr) -> PResult {
			// Try evaluating with TreeEval(Short Path).
			UNPACK_QRESULT(TreeEvalValue tree_eval =, evaluateWithTreeEval(ctx, expr));

			variant_match(tree_eval) {
				variant_case(CompileTimeValue, ctv) { return ctv; }
				variant_case(CouldNotShortPath, _) {
					// If TreeEval failed, try to evaluate with VM.
					if (const auto* reusable_expr
					    = dynamic_cast<const code::ReusableExpr*>(expr.get())) {
						// If it's a reusable expression, propagate evaluation inwards.
						return evalHoutExpr(ctx, reusable_expr->inner.ref());
					}
					if (const auto* call_expr = dynamic_cast<const code::CallExpr*>(expr.get())) {
						// If it's a call, evaluate it via the VM.
						return evaluateFunctionWithVm(ctx, call_expr);
					}

					// Otherwise, log an error.
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Evaluation of this expression in DVM at compile time",
						expr->origin.getStablePosition()
					));
					return query::Failed();
				}
				variant_default { CORE_PANIC("Unexpected TreeEvalResult variant."); }
			}
			CORE_UNREACHABLE();
		}

		static auto provide(query::Context& ctx, const QKey key) -> PResult {
			return evalHoutExpr(ctx, key.expr);
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluateHOUTExpression);

	CompTimeEvalResult getTypeCTVFromPST(
		query::Context& ctx, pst::GenericPSTQueryKey<pst::ExprElement> pst_expr
	) {
		const auto hout_qresult = getHoutOfExprWithExpectedType(
			ctx,
			pst_expr,
			tsh::SymbolType<>{
				tsh::getMetaType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			}
		);
		if (hout_qresult.hasFailed()) return query::Failed();
		return ctx.query<QueryEvaluateHOUTExpression>({ hout_qresult.valueOrThrow().ref() });
	}
}
