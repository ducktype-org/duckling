#include "expr_lowering.hpp"

#include "helios/symbols/query_type_symbol_data.hpp"
#include "mir_private/utils/slices.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <mir/mir_structure/mir_structure.hpp>

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

		void visitLiteralCharExpr(const hc::LiteralCharExpr& expr) override {
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
				auto symbol_kind = helios::kind(expr.symbol);
				CORE_ASSERT(
					symbol_kind == helios::SymbolKind::Variable
						|| symbol_kind == helios::SymbolKind::Const,
					"IdentifierExpr symbol should be either local variable or global variable or "
					"constant."
				);

				MIRGlobal::Kind global_kind = (symbol_kind == helios::SymbolKind::Const)
				                                ? MIRGlobal::Kind::Constant
				                                : MIRGlobal::Kind::Variable;

				valueOutput(
					continuation,
					MIRValue{
						MIRGlobal({
							expr.symbol,
							expr.expression_type.getSymbolType(),
							global_kind,
						}),
					}
				);
			}
		}

		void visitReusableExpr(const helios::code::ReusableExpr& expr) override {
			// If this is the subsequent use of the expression,
			// simply return the temporary value assigned to it.
			auto target_location = function.getTmpForReusableExpr(expr, expr_scope);
			if (not expr.first_use) {
				valueOutput(continuation, target_location);
				return;
			}

			// Otherwise, compute the value of the expression.
			auto assign_hole   = continuation->addHole();
			auto lowered_inner = lowerSubExpr(*expr.inner, continuation);
			lowered_inner.storeResultInGivenPlace(
				MIRPlace(target_location),
				assign_hole,
				{ flagConstruct(target_location) },
				expr_scope,
				{}
			);

			valueOutput(lowered_inner.begin, target_location);
		}

		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole = continuation->addHole();

			auto       lowered_right = lowerSubExpr(*expr.rhs, continuation);
			const auto res_right     = lowered_right.getResult(function);
			auto       lowered_left  = lowerSubExpr(*expr.lhs, lowered_right.begin);
			const auto res_left      = lowered_left.getResult(function);

			// Fill the hole with the binary operation.
			const auto      result_type = expr.expression_type.getSymbolType();
			const Operation operation   = builtinBinaryToOperation(expr.operation);

			noValueOutput(
				lowered_left.begin,
				target_construction_hole,
				Instruction(
					operation, {}, { res_left, res_right }, {}, expr_scope, {}, { expr.getPosition() }
				),
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
				Instruction(
					operation, {}, { res_lowered }, {}, expr_scope, {}, { expr.getPosition() }
				),
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
					expr_scope,
					{}
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
				{},
				{ ternary_expr.getPosition() },
			});

			// Return (always value).
			valueOutput(lowered_condition.begin, target_location);
		}

		void visitParenthesisExpr(const hc::ParenthesisExpr& expr) override {
			output(lowerSubExpr(*expr.inner, continuation));
		}

		void visitTupleExpr(const hc::TupleExpr& expr) override {
			// Tuple packing is a call to implicit tuple constructor
			auto call = continuation->addHole();

			auto                  current = continuation;
			std::vector<MIRValue> args;
			args.reserve(1 + expr.elements.size());  // ctor + each element

			auto ctor_symid = expr.tuple_ctor_symbol;
			args.emplace_back(MIRFunctionLiteral{ ctor_symid });

			for (const auto& element: expr.elements) {
				auto lowered_element = lowerSubExpr(*element, continuation);
				args.push_back(lowered_element.getResult(function));
				current = lowered_element.begin;
			}

			return noValueOutput(
				continuation,
				call,
				Instruction{ Operation::Call, {}, args, {}, expr_scope, {}, { expr.getPosition() } },
				expr.expression_type.getSymbolType()
			);
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
				Instruction(
					Operation::MetaCreateVariant,
					{},
					subtype_values,
					{},
					expr_scope,
					{},
					{ expr.getPosition() }
				),
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
			if (expr.base->expression_type.getSymbolType().getType().getKind() == tsh::Kind::Meta) {
				// @TODO: #1918 Implement that.
				throw base::NotYetImplemented("Lowering of IndexExpr operating on Meta");
			} else if (expr.base->expression_type.getSymbolType().getType().getKind()
			           == tsh::Kind::Slice) {
				auto bounds_check_fail_block = function.newBlock();
				auto bounds_check_cond_block = function.newBlock();
				auto entry_block             = function.newBlock();
				entry_block->setTerminator(Instruction{
					Operation::Jump, {}, { bounds_check_cond_block->getID() }, {}, expr_scope });

				auto lowered_index = lowerSubExpr(*expr.index, entry_block);
				auto index_val     = lowered_index.getResult(function);

				auto lowered_base = lowerSubExpr(*expr.base, lowered_index.begin);
				auto base_val     = lowered_base.getResult(function);

				// We perform bound checking
				auto slice_data = function.getContext().query<helios::QuerySliceTypeData>(
					expr.base->expression_type.getSymbolType().getType()
				);
				sliceBoundsCheck(
					{ .condition_block = bounds_check_cond_block,
				      .fail_block      = bounds_check_fail_block,
				      .ok_block        = continuation,
				      .function        = function,
				      .scope           = expr_scope },
					*slice_data,
					index_val,
					base_val,
					expr.getPosition()
				);

				variant_match(std::move(base_val.getVariant())) {
					variant_case(MIRPlace, place) {
						auto result = place.withField(function.getContext(), slice_data->ptr)
						                  .withIndex(index_val);
						valueOutput(lowered_base.begin, result);
					}
					variant_default { CORE_PANIC("Index base must be a MIRPlace"); }
				}

			} else {
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
		}

		void visitSequenceExpr(const hc::SequenceExpr&) override {
			throw base::NotYetImplemented("sequence expr lowering");
		}

		void visitChainComparisonExpr(const hc::ChainComparisonExpr& chain_expr) override {
			auto lower_subexpr_with_result
				= [this](CRef<hc::Expr> expression, BlockBuilderRef next_block) {
					  auto lowered = lowerSubExpr(*expression, next_block);
					  return std::pair{ lowered.begin, lowered.getResult(function) };
				  };

			// Place for a comparison instruction
			auto last_comparison_block = function.newBlock();

			// After the last comparison, continue regardless of the result.
			last_comparison_block->setTerminator(Instruction{
				Operation::Jump, {}, { continuation->getID() }, {}, expr_scope });

			// The result of evaluating the expression (result of the last evaluated sub-expression).
			auto boolean_output
				= function.addTmp(chain_expr.expression_type.getSymbolType(), expr_scope);

			// Now, build the proper comparisons in reverse order.
			auto  next_block = continuation;
			usize comps_left = chain_expr.comparisons.size();
			for (auto& comp: chain_expr.comparisons | std::views::reverse) {
				comps_left--;
				// First, prepare the block.
				// If the comparison is the last one (next_block == continuation), we jump to the
				// continuation regardless of the result. Otherwise, we branch to the next comparison
				// if the result is true, and to the continuation if the results is false.
				auto comparison_block = function.newBlock();
				if (next_block->getID() == continuation->getID()) {
					comparison_block->setTerminator(Instruction{
						Operation::Jump,
						{},
						{ continuation->getID() },
						{},
						expr_scope,
					});
				} else {
					comparison_block->setTerminator(Instruction{
						Operation::Branch,
						{},
						{ boolean_output, next_block->getID(), continuation->getID() },
						{},
						expr_scope,
					});
				}
				auto comparison_hole = comparison_block->addHole();

				// Now, lower the comparison
				auto [comp_cont, comp_res]
					= lower_subexpr_with_result(comp.ref(), comparison_block);

				// Finally, fill in the comparison instruction.
				// Remember to set construction flag for boolean_output only for the first comparison.
				comparison_hole.fill(Instruction{
					Operation::Assign,
					{ boolean_output },
					{ comp_res },
					comps_left == 0 ? std::vector{ flagConstruct(boolean_output) }
									: std::vector<OperationFlag>{},
					expr_scope,
					{},
					{ comp->getPosition() },
				});
				next_block = comp_cont;
			}

			// Now next_block is the starting block of the first comparison.
			valueOutput(next_block, boolean_output);
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
				Instruction{ Operation::Call, {}, args, {}, expr_scope, {}, { expr.getPosition() } },
				expr.expression_type.getSymbolType()
			);
		}

		bool isEmptyCast(const hc::CastExpr& expr) {
			if (expr.source_expr->expression_type.getSymbolType() == expr.target_type) return true;

			// If cast is from ref T to ptr T it is empty
			if (expr.source_expr->expression_type.getSymbolType().getRefKind()
			        == tsh::ReferenceKind::Ref
			    && expr.target_type.getType().getKind() == tsh::Kind::Pointer) {
				return true;
			}

			// If cast is from box T to ptr T it is empty
			if (expr.source_expr->expression_type.getSymbolType().getRefKind()
			        == tsh::ReferenceKind::Box
			    && expr.target_type.getType().getKind() == tsh::Kind::Pointer) {
				return true;
			}

			return false;
		}

		void visitCastExpr(const hc::CastExpr& expr) override {
			// Maybe in the future the cast expr can be converted into more specific instructions.
			if (isEmptyCast(expr)) {
				// If the cast doesn't change the representation, simply ignore it.
				output(lowerSubExpr(*expr.source_expr, continuation));
				return;
			}

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
			                                 .target_type = expr.target_type },
			                 { expr.getPosition() } },
				expr.expression_type.getSymbolType()
			);
		}

		void visitRefOfExpr(const hc::RefOfExpr& expr) override {
			const auto& inner_type = expr.inner->expression_type.getSymbolType();

			if (inner_type.getRefKind() == tsh::ReferenceKind::Direct) {
				auto       hole          = continuation->addHole();
				auto       lowered_inner = lowerSubExpr(*expr.inner, continuation);
				const auto res_inner     = lowered_inner.getResult(function);
				const auto result_type   = expr.expression_type.getSymbolType();

				noValueOutput(
					lowered_inner.begin,
					hole,
					Instruction(
						Operation::AddressOf,
						{},
						{ res_inner },
						{},
						expr_scope,
						{},
						{ expr.getPosition() }
					),
					result_type
				);
			} else {
				// If a reference of box or ref is taken, no `AddressOf` instruction is inserted.
				output(lowerSubExpr(*expr.inner, continuation));
			}
		}

		void visitBoxOfExpr(const hc::BoxOfExpr& expr) override {
			CORE_ASSERT(
				expr.inner->expression_type.getSymbolType().getRefKind()
					== tsh::ReferenceKind::Direct,
				"BoxOfExpr on a non direct type"
			);

			auto       hole          = continuation->addHole();
			auto       lowered_inner = lowerSubExpr(*expr.inner, continuation);
			const auto res_inner     = lowered_inner.getResult(function);
			const auto result_type   = expr.expression_type.getSymbolType();

			noValueOutput(
				lowered_inner.begin,
				hole,
				Instruction(
					Operation::BoxAlloc, {}, { res_inner }, {}, expr_scope, {}, { expr.getPosition() }
				),
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

		void visitListPushExpr(const hc::ListPushExpr& expr) override {
			auto hole         = continuation->addHole();
			auto lowered_elem = lowerSubExpr(*expr.element, continuation);
			auto elem_val     = lowered_elem.getResult(function);
			auto lowered_list = lowerSubExpr(*expr.list, continuation);
			auto list_val     = lowered_list.getResult(function);

			noValueOutput(
				lowered_list.begin,
				hole,
				Instruction(Operation::ListPush, {}, { list_val, elem_val }, {}, expr_scope),
				expr.expression_type.getSymbolType()
			);
		}

		void visitListPopExpr(const hc::ListPopExpr& expr) override {
			auto hole          = continuation->addHole();
			auto lowered_count = lowerSubExpr(*expr.count, continuation);
			auto count_val     = lowered_count.getResult(function);
			auto lowered_list  = lowerSubExpr(*expr.list, continuation);
			auto list_val      = lowered_list.getResult(function);

			noValueOutput(
				lowered_list.begin,
				hole,
				Instruction(Operation::ListPop, {}, { list_val, count_val }, {}, expr_scope),
				expr.expression_type.getSymbolType()
			);
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
					tsh::getUnitType(),
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

				tsh::SymbolType<> result_type{
					tsh::getMetaType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				return ExprLowerRes(
					current,
					ExprLowerRes::Finalizer{
						.hole = hole,
						.instr
						= Instruction(Operation::MetaCreateTuple, {}, element_types, {}, expr_scope),
						.type = result_type }
				);
			} else if (const auto* paren_expr = dynamic_cast<const hc::ParenthesisExpr*>(&expr)) {
				return lowerAndLiftToTypeRecursively(*paren_expr->inner, continuation);
			} else if (const auto* reusable_expr
			           = dynamic_cast<const helios::code::ReusableExpr*>(&expr)) {
				return lowerAndLiftToTypeRecursively(*reusable_expr->inner, continuation);
			}

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
			case Len:
				return Operation::ListLen;
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
		ScopeRef                          scope,
		InstructionMetadata               metadata
	) {
		variant_match(value) {
			variant_case(MIRValue, val) {
				hole.fill(Instruction{
					Operation::Assign, target, { val }, flags, scope, {}, metadata });
			}
			variant_case(Finalizer, res_data) {
				CORE_ASSERT(scope == res_data.instr.scope, "Scope mismatch!");

				hole.fillNop(scope);
				res_data.instr.output.emplace(target);
				res_data.instr.flags.insert(res_data.instr.flags.end(), flags.begin(), flags.end());
				res_data.instr.metadata = metadata;
				res_data.hole.fill(res_data.instr);
				value = target;
			}
		}
	}
}
