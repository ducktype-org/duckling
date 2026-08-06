#include "expr_lowering.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <mir_private/stmt_lowering.hpp>
#include <mir_private/utils/bounds_check.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <ranges>
#include <variant>

namespace compiler::mir {

	/**
	 * @brief A resolved MIR operation together with the `extra_params` it needs.
	 * Used by the builtin-operator lowering helpers, since meta operations lower to a single
	 * `Operation::MetaTypeOperation` parametrized by `MetaParameters`.
	 */
	class OperationWithParams final {
	public:
		Operation       operation;
		InstrParameters params = NoInstrParameters{};

		OperationWithParams(Operation op, InstrParameters param = NoInstrParameters{}):
			  operation(op),
			  params(param) {}
	};

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
			valueOutput(
				continuation,
				MIRValue{ MIRConstant{ ctv::CompileTimeValue::CharSliceValue{ expr.value } } }
			);
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
			const auto result_type           = expr.expression_type.getSymbolType();
			const auto operation_with_params = builtinBinaryToOperation(expr.operation);

			noValueOutput(
				lowered_left.begin,
				target_construction_hole,
				Instruction(
					operation_with_params.operation,
					{},
					{ res_left, res_right },
					{},
					expr_scope,
					operation_with_params.params,
					{ expr.getPosition() }
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

			const auto [operation, param] = builtinUnaryToOperation(expr.operation);

			noValueOutput(
				lowered.begin,
				target_construction_hole,
				Instruction(
					operation, {}, { res_lowered }, {}, expr_scope, param, { expr.getPosition() }
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

			for (const auto& element: expr.elements | std::views::reverse) {
				auto lowered_element = lowerSubExpr(*element, current);
				args.push_back(lowered_element.getResult(function));
				current = lowered_element.begin;
			}
			std::reverse(args.begin() + 1, args.end());

			return noValueOutput(
				current,
				call,
				Instruction{ Operation::Call, {}, args, {}, expr_scope, {}, { expr.getPosition() } },
				expr.expression_type.getSymbolType()
			);
		}

		template<class ProjectionFor>
		void lowerInPlaceConstruction(
			const tsh::SymbolType<>&                dest_type,
			const std::vector<base::Box<hc::Expr>>& values,
			ProjectionFor                           projection_for,
			base::Optional<dia_int::StablePosition> position
		) {
			// We create a tmp, but we don't check use before init, as the value is never inited.
			auto dest = function.addTmp(dest_type, expr_scope);
			dest->lifetime_flags |= LifetimeFlag::NoMoveStatusValidation;
			auto current = continuation;

			if (values.empty()) {
				continuation->addInstruction(Instruction(
					Operation::Nop, {}, {}, { flagConstruct(dest) }, expr_scope, {}, { position }
				));
				valueOutput(continuation, MIRValue{ dest });
				return;
			}

			// The chain is built back-to-front, so the first iteration lowers the store that runs
			// last. Only that store constructs the destination — before it, the value is still
			// partially uninitialized.
			for (usize i: std::views::iota(usize{ 0 }, values.size()) | std::views::reverse) {
				const bool is_last_store = i + 1 == values.size();

				auto element_place = projection_for(MIRPlace(dest), i);
				auto store_hole    = current->addHole();
				auto value_result  = lowerSubExpr(*values[i], current);
				value_result.storeResultInGivenPlace(
					element_place,
					store_hole,
					is_last_store ? std::vector{ flagConstruct(dest) }
								  : std::vector<OperationFlag>{},
					expr_scope,
					{ position }
				);
				current = value_result.begin;
			}

			valueOutput(current, MIRValue{ dest });
		}

		/**
		 * @brief Lowers a static array construction that got a single value for all of its
		 * elements as a loop storing that value into every element.
		 *
		 * The generated shape, where `dest` is the constructed array and `i` the counter:
		 * ```
		 * entry: i = 0                                              -> jump cond
		 * cond:  cond_tmp = i < size                                -> branch cond_tmp ? body :
		 * continuation body:  dest[i] = <value>; <per_element_body>; i = i + 1   -> jump cond
		 * ```
		 */
		void lowerArrayFillLoop(
			const tsh::SymbolType<>&                               dest_type,
			const hc::Expr&                                        value,
			usize                                                  size,
			const base::Optional<base::CSharedBox<hc::CodeBlock>>& per_element_body,
			base::Optional<dia_int::StablePosition>                position
		) {
			auto& ctx = function.getContext();

			// We create a tmp, but we don't check use before init, as the value is never inited.
			auto dest = function.addTmp(dest_type, expr_scope);
			dest->lifetime_flags |= LifetimeFlag::NoMoveStatusValidation;

			// The counter only ever goes from zero up to the size of the array, so it is unsigned.
			const auto counter_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			auto counter
				= function.addTmp(tsh::SymbolType<>::withDefaults(counter_type), expr_scope);

			auto counter_constant = [&](usize constant) {
				return MIRValue{ MIRConstant{
					ctv::CompileTimeValue{ ctv::NumericValue::createOfType(counter_type, constant)
					                           .expect("u64 numeric value creation failed") } } };
			};

			auto cond_block = function.newBlock();
			auto body_block = function.newBlock();

			// The body is assembled in reverse execution order, so the increment goes in first.
			body_block->setTerminator(
				{ Operation::Jump, {}, { cond_block->getID() }, {}, expr_scope }
			);
			body_block->addInstruction(Instruction(
				Operation::IntegerAdd,
				MIRPlace(counter),
				{ MIRValue(counter), counter_constant(1) },
				{ flagReinit(counter) },
				expr_scope,
				{},
				{ position }
			));

			// Temporaries of the value expression get their own scope, so that they are destructed
			// at the end of every iteration instead of piling up over the whole loop.
			auto body_scope = function.newScope(expr_scope);

			auto store_continuation = body_block;
			if_opt_some(per_element_body, body) {
				store_continuation = lowerCodeBlock(*body, body_block, function, body_scope).begin;
			}

			auto store_hole   = store_continuation->addHole();
			auto value_result = lowerExpr(value, store_continuation, function, body_scope);
			value_result.storeResultInGivenPlace(
				MIRPlace(dest).withIndex(MIRValue(counter)), store_hole, {}, body_scope, { position }
			);

			auto condition = function.addConditionTmp(expr_scope);
			cond_block->addInstruction(Instruction(
				Operation::IntegerLt,
				MIRPlace(condition),
				{ MIRValue(counter), counter_constant(size) },
				{ flagConstruct(condition) },
				expr_scope,
				{},
				{ position }
			));
			cond_block->setTerminator(Instruction(
				Operation::Branch,
				{},
				{ MIRValue(condition), value_result.begin->getID(), continuation->getID() },
				{},
				expr_scope,
				{},
				{ position }
			));

			// The array is only fully constructed once the loop is over, so the flag sits on the
			// first instruction executed after it.
			continuation->addInstruction(Instruction(
				Operation::Nop, {}, {}, { flagConstruct(dest) }, expr_scope, {}, { position }
			));

			auto entry_block = function.newBlock();
			entry_block->addInstruction(Instruction(
				Operation::Assign,
				MIRPlace(counter),
				{ counter_constant(0) },
				{ flagConstruct(counter) },
				expr_scope,
				{},
				{ position }
			));
			entry_block->setTerminator(
				{ Operation::Jump, {}, { cond_block->getID() }, {}, expr_scope }
			);

			valueOutput(entry_block, MIRValue{ dest });
		}

		void visitCreateAggregateExpr(const hc::CreateAggregateExpr& expr) override {
			auto& ctx = function.getContext();

			const bool is_static_array = expr.type.getKind() == tsh::Kind::StaticArray;

			// Static arrays project by index, every other aggregate projects by field.
			if (is_static_array) {
				auto array_type = expr.type.as<tsh::StaticArrayAbstractType>();

				// A single value for a multi-element static array fills all of its elements.
				if (expr.values.size() == 1 and array_type.getSize() > 1) {
					lowerArrayFillLoop(
						expr.expression_type.getSymbolType(),
						*expr.values.front(),
						array_type.getSize(),
						expr.per_element_body,
						expr.getPosition()
					);
					return;
				}

				CORE_ASSERT(
					expr.per_element_body.empty(),
					"A per-element body is only valid for a static array in a loop."
				);
				CORE_ASSERT(
					array_type.getSize() == expr.values.size(),
					"CreateAggregateExpr value count must match the static array's size, unless it "
					"is a single value filling the whole array."
				);

				lowerInPlaceConstruction(
					expr.expression_type.getSymbolType(),
					expr.values,
					[](const MIRPlace& base, usize i) {
						auto index = MIRValue{ MIRConstant{
							ctv::CompileTimeValue{ ctv::NumericValue{ static_cast<i64>(i) } } } };
						return base.withIndex(index);
					},
					expr.getPosition()
				);
				return;
			}

			CORE_ASSERT(
				expr.per_element_body.empty(),
				"A per-element body is only valid for a static array in a loop."
			);

			// Field symbols of the aggregate, in declaration order — one per value.
			auto                       interface = expr.type.getInterface(ctx);
			std::vector<helios::SymID> field_symbols;
			for (const auto& field: interface->getFieldsView())
				field_symbols.push_back(field.getSymbol());
			CORE_ASSERT(
				field_symbols.size() == expr.values.size(),
				"CreateAggregateExpr value count must match the aggregate's field count."
			);

			lowerInPlaceConstruction(
				expr.expression_type.getSymbolType(),
				expr.values,
				[&](const MIRPlace& base, usize i) -> MIRPlace {
					return base.withField(ctx, field_symbols[i]);
				},
				expr.getPosition()
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
					Operation::MetaTypeOperation,
					{},
					subtype_values,
					{},
					expr_scope,
					MetaParameters{ MetaKind::CreateVariant },
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
			const auto base_type = expr.base->expression_type.getSymbolType().getType();
			const auto base_kind = base_type.getKind();

			if (base_kind == tsh::Kind::Meta) {
				// @TODO: #1918 Implement that.
				throw base::NotYetImplemented("Lowering of IndexExpr operating on Meta");
			}

			// Slices and dynamic arrays store their data behind a `ptr` field, so indexing them is
			// `Field(ptr) -> Index`. Static arrays and many-pointers index directly.
			auto element_place = [&](const MIRPlace& place, const MIRValue& index_val) -> MIRPlace {
				auto& ctx = function.getContext();
				switch (base_kind) {
				case tsh::Kind::Slice: {
					auto slice_data = ctx.query<helios::QuerySliceTypeData>(base_type);
					return place.withField(ctx, slice_data->ptr).withIndex(index_val);
				}
				case tsh::Kind::DynamicArray: {
					auto dyn_data = ctx.query<helios::QueryDynamicArrayTypeData>(base_type);
					return place.withField(ctx, dyn_data->ptr).withIndex(index_val);
				}
				case tsh::Kind::StaticArray:
				case tsh::Kind::ManyPointer:
					return place.withIndex(index_val);
				default:
					CORE_PANIC("IndexExpr base must be an indexable type");
				}
			};

			// Many-pointers have no length, so they cannot be bounds-checked and are lowered
			// directly.
			if (base_kind == tsh::Kind::ManyPointer) {
				auto lowered_index = lowerSubExpr(*expr.index, continuation);
				auto index_val     = lowered_index.getResult(function);

				auto lowered_base = lowerSubExpr(*expr.base, lowered_index.begin);
				auto base_val     = lowered_base.getResult(function);

				variant_match(std::move(base_val.getVariant())) {
					variant_case(MIRPlace, place) {
						valueOutput(lowered_base.begin, element_place(place, index_val));
					}
					variant_default { CORE_PANIC("Index base must be a MIRPlace"); }
				}
				return;
			}

			// The length of the indexed array - slices and dynamic arrays read their `len` field,
			// static arrays use their compile-time size.
			auto array_length = [&](const MIRPlace& place) -> MIRValue {
				auto& ctx = function.getContext();
				switch (base_kind) {
				case tsh::Kind::Slice: {
					auto slice_data = ctx.query<helios::QuerySliceTypeData>(base_type);
					return place.withField(ctx, slice_data->len);
				}
				case tsh::Kind::DynamicArray: {
					auto dyn_data = ctx.query<helios::QueryDynamicArrayTypeData>(base_type);
					return place.withField(ctx, dyn_data->len);
				}
				case tsh::Kind::StaticArray: {
					const auto size = base_type.as<tsh::StaticArrayAbstractType>().getSize();
					return MIRValue{ MIRConstant{
						ctv::CompileTimeValue{ ctv::NumericValue{ static_cast<i64>(size) } } } };
				}
				default:
					CORE_PANIC("Bounds check requires a sized array type");
				}
			};

			// Now, before the index projection, perform the bounds check.
			auto bounds_check_fail_block = function.newBlock();
			auto bounds_check_cond_block = function.newBlock();
			auto entry_block             = function.newBlock();
			entry_block->setTerminator(Instruction{
				Operation::Jump, {}, { bounds_check_cond_block->getID() }, {}, expr_scope });

			auto lowered_index = lowerSubExpr(*expr.index, entry_block);
			auto index_val     = lowered_index.getResult(function);

			auto lowered_base = lowerSubExpr(*expr.base, lowered_index.begin);
			auto base_val     = lowered_base.getResult(function);

			variant_match(std::move(base_val.getVariant())) {
				variant_case(MIRPlace, place) {
					boundsCheck(
						{ .condition_block = bounds_check_cond_block,
					      .fail_block      = bounds_check_fail_block,
					      .ok_block        = continuation,
					      .function        = function,
					      .scope           = expr_scope },
						index_val,
						array_length(place),
						expr.getPosition()
					);

					valueOutput(lowered_base.begin, element_place(place, index_val));
				}
				variant_default { CORE_PANIC("Index base must be a MIRPlace"); }
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

		void visitMoveExpr(const hc::MoveExpr& expr) override {
			// `move x` yields the value of `x` and marks the source local as moved-out, so any
			// later use is flagged by the move state/use-after-move analysis. The `Move` flag has
			// to sit on an instruction that reads the local, so we copy it into a fresh temporary
			// and attach the flag there.
			auto hole          = continuation->addHole();
			auto lowered_inner = lowerSubExpr(*expr.inner, continuation);
			auto inner_val     = lowered_inner.getResult(function);

			// Moving anything that is not a plain local place (e.g. a temporary) has no source to
			// mark, so just forward the value unchanged.
			if (not inner_val.isLocal() or inner_val.get<mir::MIRPlace>().hasProjections()) {
				function.getContext().logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Moving from a non-local place is not supported yet.", expr.inner->getPosition()
				));
				query::throwFailed();
				return;
			}

			const auto moved_local = inner_val.get<MIRPlace>().getBase<MIRLocalRef>();

			noValueOutput(
				lowered_inner.begin,
				hole,
				Instruction(
					Operation::Assign,
					{},
					{ inner_val },
					{ OperationFlag{ .flag = OperationFlag::Flag::Move, .local = moved_local } },
					expr_scope,
					{},
					{ expr.getPosition() }
				),
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
			auto lowered_list = lowerSubExpr(*expr.list, lowered_elem.begin);
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
			auto lowered_list  = lowerSubExpr(*expr.list, lowered_count.begin);
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
						.hole  = hole,
						.instr = Instruction(
							Operation::MetaTypeOperation,
							{},
							element_types,
							{},
							expr_scope,
							MetaParameters{ MetaKind::CreateTuple }
						),
						.type = result_type,
					}
				);
			} else if (const auto* paren_expr = dynamic_cast<const hc::ParenthesisExpr*>(&expr)) {
				return lowerAndLiftToTypeRecursively(*paren_expr->inner, continuation);
			} else if (const auto* reusable_expr
			           = dynamic_cast<const helios::code::ReusableExpr*>(&expr)) {
				return lowerAndLiftToTypeRecursively(*reusable_expr->inner, continuation);
			}

			// Any other expression of unit type (a tuple element, a call, ...) is still lowered
			// because it may have side effects, but the unit type has a single value, so what it
			// lifts to is always the unit type itself.
			if (expr.expression_type.getType().getKind() == tsh::Kind::Unit) {
				auto lowered = lowerSubExpr(expr, continuation);
				// Materialise the result so the lowered instructions stay well formed, then drop
				// it - only its type is of interest here.
				[[maybe_unused]] const auto unit_value = lowered.getResult(function);

				tsh::SymbolType<> unit_sym_type{
					tsh::getUnitType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				return ExprLowerRes(lowered.begin, MIRValue{ MIRConstant{ unit_sym_type } });
			}

			return lowerSubExpr(expr, continuation);
		}

		static OperationWithParams builtinBinaryToOperation(const hc::BuiltinBinary builtin) {
			using enum hc::BuiltinBinary;
			switch (builtin) {
			/// Integer arithmetic ///
			case IntegerAdd:
				return { Operation::IntegerAdd };
			case IntegerSub:
				return { Operation::IntegerSub };
			case IntegerMul:
				return { Operation::IntegerMul };
			case IntegerDiv:
				return { Operation::IntegerDiv };
			case IntegerMod:
				return { Operation::IntegerMod };
			case IntegerPow:
				// @TODO: #1610 Implement exponentiation as a function call.
				throw base::NotYetImplemented("Exponentiation on variables");

			/// Integer comparisons ///
			case IntegerLt:
				return { Operation::IntegerLt };
			case IntegerGt:
				return { Operation::IntegerGt };
			case IntegerLteq:
				return { Operation::IntegerLteq };
			case IntegerGteq:
				return { Operation::IntegerGteq };
			case IntegerEq:
				return { Operation::IntegerEq };
			case IntegerNeq:
				return { Operation::IntegerNeq };

			/// Floating point arithmetic d///
			case FloatAdd:
				return { Operation::FloatAdd };
			case FloatSub:
				return { Operation::FloatSub };
			case FloatMul:
				return { Operation::FloatMul };
			case FloatDiv:
				return { Operation::FloatDiv };
			case FloatPow:
				// @TODO: #1610 Implement exponentiation as a function call.
				throw base::NotYetImplemented("Exponentiation on variables");

			/// Floating point comparisons ///
			case FloatLt:
				return { Operation::FloatLt };
			case FloatGt:
				return { Operation::FloatGt };
			case FloatLteq:
				return { Operation::FloatLteq };
			case FloatGteq:
				return { Operation::FloatGteq };
			case FloatEq:
				return { Operation::FloatEq };
			case FloatNeq:
				return { Operation::FloatNeq };

			case MetaEq:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::Eq } };
			case MetaNeq:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::Neq } };

			case BooleanAnd:
				return { Operation::BooleanAnd };
			case BooleanOr:
				return { Operation::BooleanOr };
			default:
				CORE_UNREACHABLE();
			}
		}

		static OperationWithParams builtinUnaryToOperation(const hc::BuiltinUnary builtin) {
			using enum hc::BuiltinUnary;
			switch (builtin) {
			case IntegerNegation:
				return { Operation::IntegerNeg };
			case FloatNegation:
				return { Operation::FloatNeg };
			case BooleanNot:
				return { Operation::BooleanNot };
			case Box:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreateBox } };
			case Ref:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreateRef } };
			case Const:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreateConst } };
			case Ptr:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreatePtr } };
			case ManyPtr:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreateManyPtr } };
			case CPtr:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreateCPtr } };
			case Slice:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::CreateSlice } };
			case SizeOf:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::SizeOf } };
			case AlignOf:
				return { Operation::MetaTypeOperation, MetaParameters{ MetaKind::AlignOf } };
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
