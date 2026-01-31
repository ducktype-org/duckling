#include "expr_lowering.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <ranges>
#include <variant>

namespace compiler::mir {

	/**
	 * @brief Visitor that implements actual logic of lowering expression.
	 * @note The result of the visitor is stored in out member. To store expr
	 * result somewhere, call finalize with place to store it
	 */
	struct ExprBlockVisitor final: public hc::HoutExprVisitor {
		BlockBuilderRef continuation;

		base::Optional<ExprLowerRes> out;

		FunctionBuilder& function;

		/**
		 * The scope of the expression, where it and its result should live in.
		 */
		ScopeRef expr_scope;

		ExprBlockVisitor(
			BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope
		):
			  continuation(continuation),
			  function(function),
			  expr_scope(expr_scope) {}

		void output(ExprLowerRes&& lowering_result) {
			CORE_ASSERT(out.empty(), "Output already set.");
			out.emplace(std::move(lowering_result));
		}

		void valueOutput(BlockBuilderRef begin, const MIRValue& value) {
			CORE_ASSERT(out.empty(), "Output already set.");
			out.emplace(ExprLowerRes(begin, value));
		}

		void noValueOutput(
			BlockBuilderRef                      begin,
			const BlockBuilder::InstructionHole& hole,
			const Instruction&                   instr,
			const tsh::SymbolType<>&             type
		) {
			CORE_ASSERT(out.empty(), "Output already set.");
			CORE_ASSERT(instr.output.empty(), "instruction shouldn't have output set.");
			out.emplace(ExprLowerRes(begin, ExprLowerRes::Finalizer(hole, instr, type)));
		}

		ExprLowerRes lowerSubExpr(const hc::Expr& expr, BlockBuilderRef continuation) {
			return lowerExpr(expr, continuation, function, expr_scope);
		}

		void visitLiteralUnitExpr(const hc::LiteralUnitExpr&) override {
			valueOutput(continuation, MIRValue{ MIRConstant{ ctv::CompileTimeValue::UnitCTV() } });
		}

		void visitLiteralNumericExpr(const hc::LiteralNumericExpr& value) override {
			valueOutput(continuation, MIRValue{ MIRConstant{ value.value } });
		}

		void visitLiteralBoolExpr(const hc::LiteralBoolExpr& expr) override {
			valueOutput(continuation, MIRValue{ MIRConstant{ expr.value } });
		}

		void visitLiteralStringExpr(const hc::LiteralStringExpr& expr) override {
			valueOutput(continuation, MIRValue{ MIRConstant{ expr.value } });
		}

		void visitLiteralTypeExpr(const hc::LiteralTypeExpr& expr) override {
			valueOutput(continuation, MIRValue{ MIRConstant{ expr.value_type } });
		}

		void visitIdentifierExpr(const hc::IdentifierExpr& expr) override {
			auto optional_local = function.findLocal(expr.symbol);

			if (optional_local.has_value()) {
				valueOutput(continuation, MIRValue{ optional_local.value() });
			} else {
				//@TODO: #1334 Check if the symbol is a real global variable.
				valueOutput(
					continuation,
					MIRValue{ MIRGlobal({ expr.symbol, expr.expression_type.getSymbolType() }) }
				);
			}
		}

		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole = continuation->addHole();

			auto       lowered_right = lowerSubExpr(*expr.rhs, continuation);
			const auto res_right     = lowered_right.getResult(function);
			auto       lowered_left  = lowerSubExpr(*expr.lhs, lowered_right.begin);
			const auto res_left      = lowered_left.getResult(function);

			// Fill the hole with the binary operation.
			// Assume (for now?) that the arguments are of the same type,
			// and the result is of the same type as the arguments.
			const auto argument_type       = typeOfMIRValue(res_right, function.getContext());
			const auto other_argument_type = typeOfMIRValue(res_left, function.getContext());
			CORE_ASSERT(
				argument_type.getType() == other_argument_type.getType(),
				base::strConcat(
					"Binary operator with different argument types. Left side is: '",
					argument_type.toString(),
					"' Right side is: '",
					other_argument_type.toString(),
					"'"
				)
			);
			const auto      result_type = expr.expression_type.getSymbolType();
			const Operation operation   = builtinBinaryToOperation(expr.operation);

			noValueOutput(
				lowered_left.begin,
				target_construction_hole,
				Instruction(operation, {}, { res_left, res_right }, {}, expr_scope),
				result_type
			);
		}

		void visitUnaryOperatorExpr(const hc::UnaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto       target_construction_hole = continuation->addHole();
			auto       lowered                  = lowerSubExpr(*expr.expr, continuation);
			const auto res_lowered              = lowered.getResult(function);

			const auto result_type = expr.expression_type.getSymbolType();

			const Operation operation = builtinUnaryToOperation(expr.operation);

			noValueOutput(
				lowered.begin,
				target_construction_hole,
				Instruction(operation, {}, { res_lowered }, {}, expr_scope),
				result_type
			);
		}

		void visitTernaryOperatorExpr(const helios::code::TernaryOperatorExpr& ternary_expr
		) override {
			// Get info about the target.
			const auto result_type     = ternary_expr.expression_type.getSymbolType();
			const auto target_location = function.addTmp(result_type, expr_scope);

			auto build_case_block = [this, &target_location](hc::Expr& case_expr) {
				auto block = function.newBlock();
				block->setTerminator(
					{ Operation::Jump, {}, { continuation->getID() }, {}, expr_scope }
				);
				auto assign_hole = block->addHole();

				auto lowered_block = lowerSubExpr(case_expr, block);

				lowered_block.storeResultInGivenPlace(
					MIRPlace(target_location),
					assign_hole,
					{ flagConstruct(target_location) },
					expr_scope
				);


				return lowered_block.begin;
			};

			auto else_block = build_case_block(*ternary_expr.if_false);
			auto then_block = build_case_block(*ternary_expr.if_true);

			// Build branching.
			auto condition_block   = function.newBlock();
			auto lowered_condition = lowerSubExpr(*ternary_expr.condition, condition_block);


			condition_block->setTerminator({
				Operation::Branch,
				{},
				{ lowered_condition.getResult(function), then_block->getID(), else_block->getID() },
				{},
				expr_scope,
			});

			// Return (always value).
			valueOutput(lowered_condition.begin, target_location);
		}

		void visitParenthesisExpr(const hc::ParenthesisExpr& expr) override {
			output(lowerSubExpr(*expr.inner, continuation));
		}

		void visitTupleExpr(const hc::TupleExpr&) override {
			throw base::NotYetImplemented("tuple constructor");
		}

		void visitVariantTypeConstructorExpr(const hc::VariantTypeConstructorExpr& expr) override {
			auto result_type = expr.expression_type.getSymbolType();
			CORE_ASSERT(
				result_type.getType().getKind() == tsh::Kind::Meta,
				"Expression type in Variant Type Constructor should be meta"
			);

			auto hole = continuation->addHole();

			BlockBuilderRef       current = continuation;
			std::vector<MIRValue> subtype_values;
			subtype_values.reserve(expr.subtypes.size());

			for (const auto& element: expr.subtypes | std::views::reverse) {
				auto elem_lowered = lowerSubExpr(*element, current);
				subtype_values.push_back(elem_lowered.getResult(function));
				current = elem_lowered.begin;
			}
			std::ranges::reverse(subtype_values);

			noValueOutput(
				current,
				hole,
				Instruction(Operation::MetaCreateVariant, {}, subtype_values, {}, expr_scope),
				result_type
			);
			return;
		}

		void visitAccessExpr(const hc::AccessExpr& expr) override {
			auto       sub_result = lowerSubExpr(*expr.base, continuation);
			const auto sub_begin  = sub_result.begin;
			auto       sub_value  = sub_result.getResult(function);

			variant_match(std::move(sub_value.getVariant())) {
				variant_case(MIRPlace, place) {
					valueOutput(sub_begin, place.withField(function.getContext(), expr.field));
				}
				variant_default {
					// Access base is not a place.
					CORE_UNREACHABLE();
				}
			}
		}

		void visitIndexExpr(const hc::IndexExpr& expr) override {
			auto lowered_index = lowerSubExpr(*expr.index, continuation);
			auto index_val     = lowered_index.getResult(function);

			auto lowered_base = lowerSubExpr(*expr.base, lowered_index.begin);
			auto base_val     = lowered_base.getResult(function);

			variant_match(std::move(base_val.getVariant())) {
				variant_case(MIRPlace, place) {
					valueOutput(lowered_base.begin, place.withIndex(index_val));
				}
				variant_default { CORE_PANIC("Index base must be a MIRPlace"); }
			}
		}

		void visitSequenceExpr(const hc::SequenceExpr&) override {
			throw base::NotYetImplemented("sequence expr lowering");
		}

		void visitChainComparisonExpr(const hc::ChainComparisonExpr& chain_expr) override {
			CORE_ASSERT(!chain_expr.expressions.empty(), "Empty chain comparison");
			CORE_ASSERT(chain_expr.expressions.size() != 1, "Single element chain comparison");
			CORE_ASSERT(
				chain_expr.operators.size() == chain_expr.expressions.size() - 1,
				"Operands: " + std::to_string(chain_expr.operators.size())
					+ " expressions: " + std::to_string(chain_expr.expressions.size())
					+ ", but expected one less operator then expression."
			);

			auto lower_subexpr_with_result
				= [this](CRef<hc::Expr> expression, BlockBuilderRef next_block) {
					  auto lowered = lowerSubExpr(*expression, next_block);
					  return std::pair{ lowered.begin, lowered.getResult(function) };
				  };

			using namespace std::views;

			// Place for a comparison instruction
			auto last_comparison_block = function.newBlock();
			auto prev_cmp_hole         = last_comparison_block->addHole();

			// After the last comparison, continue regardless of the result.
			last_comparison_block->setTerminator(Instruction{
				Operation::Jump, {}, { continuation->getID() }, {}, expr_scope });

			// The result of evaluating the expression (result of the last evaluated sub-expression).
			auto boolean_output
				= function.addTmp(chain_expr.expression_type.getSymbolType(), expr_scope);

			// The left-over value. We maintain that this has to partake in only one comparison,
			// which will be placed in prev_cmp_hole.boolean
			auto [prev_block, prev_value] = lower_subexpr_with_result(
				chain_expr.expressions.back().ref(), last_comparison_block
			);

			auto mir_operators = chain_expr.operators | transform(builtinBinaryToOperation);

			// First and last expressions require special handling. We build them in reverse, as usual.
			auto expressions = chain_expr.expressions | drop(1) | reverse | drop(1);
			auto comparisons = mir_operators | drop(1) | reverse;

			for (const auto& [expr, comp]: zip(expressions, comparisons)) {
				// Place for the next comparison.
				BlockBuilderRef new_comparison_block = function.newBlock();
				auto            new_cmp_hole         = new_comparison_block->addHole();
				new_comparison_block->setTerminator(Instruction{
					Operation::Branch,
					{},
					{ boolean_output, prev_block->getID(), continuation->getID() },
					{},
					expr_scope });  // We evaluate prev_value only after this comparison is true, as
				                    // prev_cmp will be the first comparison it is a part of.

				// Next expression (completes the prev_cmp).
				auto [new_block, new_value]
					= lower_subexpr_with_result(expr.ref(), new_comparison_block);

				// We create the prev_cmp, as we only now have both expressions.
				prev_cmp_hole.fill(Instruction{
					comp, { boolean_output }, { new_value, prev_value }, {}, expr_scope });

				prev_block    = new_block;
				prev_cmp_hole = new_cmp_hole;

				// expr_result participated in the previous comparison fulfilling the invariant.
				prev_value = new_value;
			}

			// The first expression to be evaluated.
			auto [first_block, first_value]
				= lower_subexpr_with_result(chain_expr.expressions.front().ref(), prev_block);

			// The first comparison to be performed.
			prev_cmp_hole.fill(Instruction{ mir_operators.front(),
			                                { boolean_output },
			                                { first_value, prev_value },
			                                { flagConstruct(boolean_output) },
			                                expr_scope });

			valueOutput(first_block, boolean_output);
		}

		void visitCallExpr(const hc::CallExpr& expr) override {
			auto call = continuation->addHole();

			auto                  sub_continuation = continuation;
			std::vector<MIRValue> args;

			auto function_symid = helios::getIdentifierExprSymID(expr.callee.ref());
			if (not function_symid.has_value()) {
				CORE_PANIC(
					"Call expression where callee is not an identifier expression is currently not "
					"supported."
				);
			}
			args.emplace_back(MIRFunctionLiteral{ function_symid.value() });
			for (const auto& arg: expr.arguments) {
				auto arg_lowered = lowerSubExpr(*arg, sub_continuation);

				args.push_back(arg_lowered.getResult(function));
				sub_continuation = arg_lowered.begin;
			}

			// @TODO: #505 here in the future we (probably) will have to handle
			// move operations related to the passing of the arguments to the function

			return noValueOutput(
				sub_continuation,
				call,
				Instruction{
					Operation::Call,
					{},
					args,
					{},
					expr_scope,
				},
				expr.expression_type.getSymbolType()
			);
		}

		void visitCastExpr(const hc::CastExpr& expr) override {
			auto       cast        = continuation->addHole();
			auto       lowered     = lowerSubExpr(*expr.source_expr, continuation);
			const auto res_lowered = lowered.getResult(function);

			return noValueOutput(
				lowered.begin,
				cast,
				Instruction{ Operation::Cast,
			                 {},
			                 { res_lowered },
			                 {},
			                 expr_scope,
			                 CastParameters{ .source_type
			                                 = expr.source_expr->expression_type.getSymbolType(),
			                                 .target_type = expr.target_type } },
				expr.expression_type.getSymbolType()
			);
		}

		void visitRefOfExpr(const hc::RefOfExpr& expr) override {
			auto       hole          = continuation->addHole();
			auto       lowered_inner = lowerSubExpr(*expr.inner, continuation);
			const auto res_inner     = lowered_inner.getResult(function);
			const auto result_type   = expr.expression_type.getSymbolType();

			noValueOutput(
				lowered_inner.begin,
				hole,
				Instruction(Operation::AddressOf, {}, { res_inner }, {}, expr_scope),
				result_type
			);
		}

		void visitDerefExpr(const hc::DerefExpr& expr) override {
			auto lowered_inner = lowerSubExpr(*expr.inner, continuation);
			auto value         = lowered_inner.getResult(function);

			variant_match(std::move(value.getVariant())) {
				variant_case(MIRPlace, place) {
					valueOutput(lowered_inner.begin, place.withDeref());
				}
				variant_default {
					// Deref base is not a place.
					CORE_UNREACHABLE();
				}
			}
		}

		void visitDefaultValueExpr(const hc::DefaultValueExpr& expr) override {
			auto hole = continuation->addHole();
			noValueOutput(
				continuation,
				hole,
				Instruction(Operation::ZeroInitialize, {}, {}, {}, expr_scope),
				expr.expression_type.getSymbolType()
			);
		}

		void visitLiftToTypeExpr(const hc::LiftToTypeExpr& expr) override {
			auto result = lowerAndLiftToTypeRecursively(*expr.value_expr, continuation);
			valueOutput(result.begin, result.getResult(function));
		}


	private:
		/**
		 * @brief Recursive helper used to lift expressions to meta-types, if they are wrapped in
		 * LiftToTypeExpr. Handles specific HOUT nodes that construct meta-types (Tuple, Variant,
		 * Unit). Other nodes are delegated back to the standard expression lowerer.
		 */
		ExprLowerRes lowerAndLiftToTypeRecursively(
			const hc::Expr& expr, BlockBuilderRef continuation
		) {
			if (const auto* _ = dynamic_cast<const hc::LiteralUnitExpr*>(&expr)) {
				tsh::SymbolType<> unit_sym_type{
					function.getContext().query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				return ExprLowerRes(continuation, MIRValue{ MIRConstant{ unit_sym_type } });
			} else if (const auto* tuple_expr = dynamic_cast<const hc::TupleExpr*>(&expr)) {
				auto                  hole    = continuation->addHole();
				BlockBuilderRef       current = continuation;
				std::vector<MIRValue> element_types;
				element_types.reserve(tuple_expr->elements.size());

				for (const auto& element: tuple_expr->elements | std::views::reverse) {
					auto elem_result = lowerAndLiftToTypeRecursively(*element, current);
					element_types.push_back(elem_result.getResult(function));
					current = elem_result.begin;
				}
				std::ranges::reverse(element_types);

				tsh::SymbolType<> result_type{ function.getContext().query<tsh::QueryMetaType>({}),
					                           tsh::ReferenceKind::Direct,
					                           tsh::Mutability::Mutable };

				return ExprLowerRes(
					current,
					ExprLowerRes::Finalizer{
						.hole = hole,
						.instr
						= Instruction(Operation::MetaCreateTuple, {}, element_types, {}, expr_scope),
						.type = result_type }
				);
			} else if (const auto* paren_expr = dynamic_cast<const hc::ParenthesisExpr*>(&expr))
				return lowerAndLiftToTypeRecursively(*paren_expr->inner, continuation);

			return lowerSubExpr(expr, continuation);
		}

		static Operation builtinBinaryToOperation(const hc::BuiltinBinary builtin) {
			using enum hc::BuiltinBinary;
			switch (builtin) {
			/// Integer arithmetic ///
			case IntegerAdd:
				return Operation::IntegerAdd;
			case IntegerSub:
				return Operation::IntegerSub;
			case IntegerMul:
				return Operation::IntegerMul;
			case IntegerDiv:
				return Operation::IntegerDiv;
			case IntegerMod:
				return Operation::IntegerMod;
			case IntegerPow:
				// @TODO: #1610 Implement exponentiation as a function call.
				throw base::NotYetImplemented("Exponentiation on variables");

			/// Integer comparisons ///
			case IntegerLt:
				return Operation::IntegerLt;
			case IntegerGt:
				return Operation::IntegerGt;
			case IntegerLteq:
				return Operation::IntegerLteq;
			case IntegerGteq:
				return Operation::IntegerGteq;
			case IntegerEq:
				return Operation::IntegerEq;
			case IntegerNeq:
				return Operation::IntegerNeq;

			/// Floating point arithmetic d///
			case FloatAdd:
				return Operation::FloatAdd;
			case FloatSub:
				return Operation::FloatSub;
			case FloatMul:
				return Operation::FloatMul;
			case FloatDiv:
				return Operation::FloatDiv;
			case FloatPow:
				// @TODO: #1610 Implement exponentiation as a function call.
				throw base::NotYetImplemented("Exponentiation on variables");

			/// Floating point comparisons ///
			case FloatLt:
				return Operation::FloatLt;
			case FloatGt:
				return Operation::FloatGt;
			case FloatLteq:
				return Operation::FloatLteq;
			case FloatGteq:
				return Operation::FloatGteq;
			case FloatEq:
				return Operation::FloatEq;
			case FloatNeq:
				return Operation::FloatNeq;

			case MetaEq:
				return Operation::MetaEq;
			case MetaNeq:
				return Operation::MetaNeq;

			case BooleanAnd:
				return Operation::BooleanAnd;
			case BooleanOr:
				return Operation::BooleanOr;
			default:
				CORE_UNREACHABLE();
			}
		}

		static Operation builtinUnaryToOperation(const hc::BuiltinUnary builtin) {
			using enum hc::BuiltinUnary;
			switch (builtin) {
			case IntegerNegation:
				return Operation::IntegerNeg;
			case FloatNegation:
				return Operation::FloatNeg;
			case BooleanNot:
				return Operation::BooleanNot;
			case Box:
				return Operation::MetaCreateBox;
			case Ref:
				return Operation::MetaCreateRef;
			case Const:
				return Operation::MetaCreateConst;
			default:
				CORE_UNREACHABLE();
			}
		}

		/**
		 * Get the type of a MIR value.
		 * @param value A MIR value.
		 * @param ctx The query context for AbstractType generation.
		 * @return The type of the local value.
		 */
		static tsh::SymbolType<> typeOfMIRValue(const MIRValue& value, query::Context& ctx) {
			variant_match(value.getVariant()) {
				variant_case(MIRConstant, constant) {
					return constant.value.getTypeOfStoredValue(ctx);
				}
				variant_case(MIRPlace, place) { return place.type; }
				variant_default { CORE_UNREACHABLE(); }
			}
			CORE_UNREACHABLE();
		}
	};

	ExprLowerRes lowerExpr(
		const hc::Expr&  expr,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         expr_scope
	) {
		ExprBlockVisitor visitor{ continuation, function, expr_scope };
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes::ExprLowerRes(BlockBuilderRef begin, std::variant<MIRValue, Finalizer> value):
		  begin{ begin },
		  value{ std::move(value) } {}

	[[nodiscard]]
	tsh::SymbolType<> ExprLowerRes::getResultType() {
		variant_match(value) {
			variant_case(Finalizer, res_data) { return res_data.type; }
			variant_default { CORE_PANIC("Function can be run only if MIRValue is not stored"); }
		}
		CORE_UNREACHABLE();
	}

	[[nodiscard]]
	base::Optional<MIRValue> ExprLowerRes::getResultIfStored() {
		variant_match(value) {
			variant_case(MIRValue, val) { return val; }
			variant_case_novalue(Finalizer) { return std::nullopt; }
		}
		CORE_UNREACHABLE();
	}

	[[nodiscard]]
	MIRValue ExprLowerRes::getResult(FunctionBuilder& function) {
		variant_match(value) {
			variant_case(MIRValue, val) { return val; }
			variant_case(Finalizer, res_data) {
				auto result = function.addTmp(res_data.type, res_data.instr.scope);
				res_data.instr.output.emplace(result);
				res_data.instr.flags.push_back(flagConstruct(result));
				res_data.hole.fill(res_data.instr);
				value = result;
				return result;
			}
		}
		CORE_UNREACHABLE();
	}

	void ExprLowerRes::storeResultInGivenPlace(
		const MIRPlace&                   target,
		BlockBuilder::InstructionHole&    hole,
		const std::vector<OperationFlag>& flags,
		ScopeRef                          scope
	) {
		variant_match(value) {
			variant_case(MIRValue, val) {
				hole.fill(Instruction{
					Operation::Assign,
					target,
					{ val },
					flags,
					scope,
				});
			}
			variant_case(Finalizer, res_data) {
				CORE_ASSERT(scope == res_data.instr.scope, "Scope mismatch!");

				hole.fillNop(scope);
				res_data.instr.output.emplace(target);
				res_data.instr.flags.insert(res_data.instr.flags.end(), flags.begin(), flags.end());
				res_data.hole.fill(res_data.instr);
				value = target;
			}
		}
	}
}
