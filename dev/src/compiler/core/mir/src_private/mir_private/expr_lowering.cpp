#include "expr_lowering.hpp"

#include "mir_builders.hpp"

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <query_framework/query_impl.hpp>

#include <ranges>
#include <variant>

namespace compiler::mir {
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

	void ExprLowerRes::storeResultInGivenVariable(
		const std::variant<LocalRef, MirGlobal>& target,
		BlockBuilder::InstructionHole&           hole,
		const std::vector<OperationFlag>&        flags,
		ScopeRef                                 scope
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
				std::visit([&](auto&& val) { res_data.instr.output.emplace(val); }, target);
				res_data.hole.fill(res_data.instr);
				res_data.instr.flags.insert(res_data.instr.flags.end(), flags.begin(), flags.end());
				std::visit([&](auto&& val) { value = val; }, target);
			}
		}
	}


	ExprBlockVisitor::ExprBlockVisitor(
		BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope
	):
		  continuation(continuation),
		  function(function),
		  expr_scope(expr_scope) {}

	void ExprBlockVisitor::output(ExprLowerRes&& lowering_result) {
		CORE_ASSERT(out.empty(), "Output already set.");
		out.emplace(std::move(lowering_result));
	}

	void ExprBlockVisitor::valueOutput(BlockBuilderRef begin, const MIRValue& value) {
		CORE_ASSERT(out.empty(), "Output already set.");
		out.emplace(ExprLowerRes(begin, value));
	}

	void ExprBlockVisitor::noValueOutput(
		BlockBuilderRef                      begin,
		const BlockBuilder::InstructionHole& hole,
		const Instruction&                   instr,
		const tsh::SymbolType<>&             type
	) {
		CORE_ASSERT(out.empty(), "Output already set.");
		CORE_ASSERT(instr.output.empty(), "instruction shouldn't have output set.");
		out.emplace(ExprLowerRes(begin, ExprLowerRes::Finalizer(hole, instr, type)));
	}

	void ExprBlockVisitor::visitLiteralIntExpr(const hc::LiteralIntExpr& expr) {
		valueOutput(continuation, MIRValue{ MirIntegerConst{ expr.value } });
	}

	void ExprBlockVisitor::visitLiteralBoolExpr(const hc::LiteralBoolExpr& expr) {
		valueOutput(continuation, MIRValue{ MirBoolConst{ expr.value } });
	}

	void ExprBlockVisitor::visitLiteralStringExpr(const hc::LiteralStringExpr&) {
		throw base::NotYetImplemented("string literal");
	}

	void ExprBlockVisitor::visitLiteralTypeExpr(const hc::LiteralTypeExpr&) {
		throw base::NotYetImplemented("type literal");
	}

	void ExprBlockVisitor::visitIdentifierExpr(const hc::IdentifierExpr& expr) {
		auto optional_local = function.findLocal(expr.symbol);

		if (optional_local.has_value()) {
			valueOutput(continuation, MIRValue{ optional_local.value() });
		} else {
			//@TODO: chack if the symbol is a real global variable.
			valueOutput(
				continuation,
				MIRValue{ MirGlobal({ expr.symbol, expr.expression_type.getSymbolType() }) }
			);
		}
	}

	void ExprBlockVisitor::visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) {
		// Construct the result of the expression in reverse.
		auto target_construction_hole = continuation->addHole();

		auto       lowered_right = lowerExpr(*expr.rhs, continuation, function, expr_scope);
		const auto res_right     = lowered_right.getResult(function);
		auto       lowered_left  = lowerExpr(*expr.lhs, lowered_right.begin, function, expr_scope);
		const auto res_left      = lowered_left.getResult(function);

		// Fill the hole with the binary operation.
		// Assume (for now?) that the arguments are of the same type,
		// and the result is of the same type as the arguments.
		const auto argument_type       = locationType(res_right, function.getContext());
		const auto other_argument_type = locationType(res_left, function.getContext());
		CORE_ASSERT(
			argument_type.getType() == other_argument_type.getType(),
			"Binary operator with different argument types"
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

	void ExprBlockVisitor::visitUnaryOperatorExpr(const hc::UnaryOperatorExpr& expr) {
		// Construct the result of the expression in reverse.
		auto       target_construction_hole = continuation->addHole();
		auto       lowered     = lowerExpr(*expr.expr, continuation, function, expr_scope);
		const auto res_lowered = lowered.getResult(function);

		const auto result_type = expr.expression_type.getSymbolType();

		const Operation operation = builtinUnaryToOperation(expr.operation);

		noValueOutput(
			lowered.begin,
			target_construction_hole,
			Instruction(operation, {}, { res_lowered }, {}, expr_scope),
			result_type
		);
	}

	void ExprBlockVisitor::visitTernaryOperatorExpr(
		const helios::code::TernaryOperatorExpr& ternary_expr
	) {
		// Get info about the target.
		const auto result_type     = ternary_expr.expression_type.getSymbolType();
		const auto target_location = function.addTmp(result_type, expr_scope);

		auto build_case_block = [this, &target_location](hc::Expr& case_expr) {
			auto block = function.newBlock();
			block->setTerminator({ Operation::Jump, {}, { continuation->getID() }, {}, expr_scope });
			auto assign_hole = block->addHole();

			auto lowered_block = lowerExpr(case_expr, block, function, expr_scope);

			lowered_block.storeResultInGivenVariable(
				target_location, assign_hole, { flagConstruct(target_location) }, expr_scope
			);


			return lowered_block.begin;
		};

		auto else_block = build_case_block(*ternary_expr.if_false);
		auto then_block = build_case_block(*ternary_expr.if_true);

		// Build branching.
		auto condition_block = function.newBlock();
		auto lowered_condition
			= lowerExpr(*ternary_expr.condition, condition_block, function, expr_scope);


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

	void ExprBlockVisitor::visitParenthesisExpr(const hc::ParenthesisExpr& expr) {
		output(lowerExpr(*expr.inner, continuation, function, expr_scope));
	}

	void ExprBlockVisitor::visitTupleTypeConstructorExpr(const hc::TupleTypeConstructorExpr&) {
		throw base::NotYetImplemented("tuple constructor");
	}

	void ExprBlockVisitor::visitVariantTypeConstructorExpr(const hc::VariantTypeConstructorExpr&) {
		throw base::NotYetImplemented("variant constructor");
	}

	void ExprBlockVisitor::visitAccessExpr(const hc::AccessExpr&) {
		throw base::NotYetImplemented("access expr lowering");
	}

	void ExprBlockVisitor::visitSequenceExpr(const hc::SequenceExpr&) {
		throw base::NotYetImplemented("sequence expr lowering");
	}

	void ExprBlockVisitor::visitChainComparisonExpr(const hc::ChainComparisonExpr& chain_expr) {
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
				  auto lowered = lowerExpr(*expression, next_block, function, expr_scope);
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

		// The left-over value. We mantain that this has to partake in only one comparison,
		// which will be placed in prev_cmp_hole.boolean
		auto [prev_block, prev_value]
			= lower_subexpr_with_result(chain_expr.expressions.back().ref(), last_comparison_block);

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
				expr_scope });  // We exaluate prev_value only after this comparison is true, as
			                    // prev_cmp will be the first comparison it is a part of.

			// Next expression (completes the prev_cmp).
			auto [new_block, new_value]
				= lower_subexpr_with_result(expr.ref(), new_comparison_block);

			// We create the prev_cmp, as we only now have both expressions.
			prev_cmp_hole.fill(Instruction{
				comp, { boolean_output }, { new_value, prev_value }, {}, expr_scope });

			prev_block    = new_block;
			prev_cmp_hole = new_cmp_hole;

			// expr_result participated in the previous comparion fulfilling the invariant.
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

	void ExprBlockVisitor::visitCallExpr(const hc::CallExpr& expr) {
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
		args.emplace_back(MirFunctionLiteral{ function_symid.value() });
		for (const auto& arg: expr.arguments) {
			auto arg_lowered = lowerExpr(*arg, sub_continuation, function, expr_scope);

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

	Operation ExprBlockVisitor::builtinBinaryToOperation(const hc::BuiltinBinary builtin) {
		using enum hc::BuiltinBinary;
		switch (builtin) {
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
			// @fixme: Implement exponentiation as a function call.
			throw base::NotYetImplemented("Exponentiation on variables");
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
		case BooleanAnd:
			return Operation::BooleanAnd;
		case BooleanOr:
			return Operation::BooleanOr;
		default:
			CORE_UNREACHABLE();
		}
	}

	Operation ExprBlockVisitor::builtinUnaryToOperation(const hc::BuiltinUnary builtin) {
		using enum hc::BuiltinUnary;
		switch (builtin) {
		case IntegerNegation:
			return Operation::IntegerNeg;
		case BooleanNot:
			return Operation::BooleanNot;
		default:
			CORE_UNREACHABLE();
		}
	}

	tsh::SymbolType<> ExprBlockVisitor::locationType(const MIRValue location, query::Context& ctx) {
		variant_match(location.getVariant()) {
			variant_case_novalue(MirIntegerConst) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryIntegralType>({ 64 }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case_novalue(MirBoolConst) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryBoolType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case(LocalRef, local) { return local->type; }
			variant_case(MirGlobal, global) { return global.type; }
			variant_default { CORE_UNREACHABLE(); }
		}
		CORE_UNREACHABLE();
	}
}
