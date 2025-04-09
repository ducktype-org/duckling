/**
 * @file mir_lowering.cpp
 * @brief Implementation of lowering HOUT functions to MIR functions.
 * The creation of MIR is done "in reverse" that is from function end to its beginning.
 */

#include "mir_lowering.hpp"

#include "mir/mir_structure/mir_structure.hpp"
#include "mir_lifetimes.hpp"

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>

namespace compiler::mir {

	namespace hc = helios::code;

	base::HashT KeyOf_LowerToMirFunction::customPerfectHash() const {
		return base::perfectHash(function);
	}

	struct InstructionHole;
	struct BlockBuilder;
	struct FunctionBuilder;

	// @TODO: since BlockBuilderRef can be a parameter
	// we will need to add BlockBuilderRef->BlockRef transformation
	// during building phase
	using BlockBuilderRef = Ref<BlockBuilder>;

	/**
	 * @brief Represents result of expression lowering, which is
	 * a BlockBuilderRef that is the beginning of the lowered expression and MIRValue
	 * that holds the result of the expression.
	 */
	struct ExprLowerRes final {
		BlockBuilderRef begin;
		MIRValue        value;
	};

	/**
	 * @brief Represents result of statement lowering, which is
	 * a BlockBuilderRef that is the beginning of the lowered statement.
	 */
	struct StmtLowerRes final {
		BlockBuilderRef begin;
	};

	/**
	 * @brief Lowers statement.
	 *
	 * @param stmt
	 * @param continuation Block that should be executed after this statement.
	 * @param function Function that we are lowering this statement in.
	 * @return StmtLowerRes
	 */
	StmtLowerRes
		lowerStmt(const hc::Stmt& stmt, BlockBuilderRef continuation, FunctionBuilder& function);

	/**
	 * @brief Lowers expression.
	 *
	 * @param expr
	 * @param continuation Block that should be executed after this expression.
	 * @param function Function that we are lowering this expression in.
	 * @return ExprLowerRes
	 */
	ExprLowerRes
		lowerExpr(const hc::Expr& expr, BlockBuilderRef continuation, FunctionBuilder& function);

	/**
	 * @brief Lowers code-block, by lowering all statements in the block.
	 *
	 * @param code_block
	 * @param continuation Block that should be executed after this code block.
	 * @param function Function that we are lowering this code block in.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block, BlockBuilderRef continuation, FunctionBuilder& function
	);

	/**
	 * @brief Creates construct flag for given local.
	 * @note: It is a function, not a constructor to avoid .hpp bloat.
	 * @param local
	 * @return constexpr OperationFlag
	 */
	constexpr OperationFlag flagConstruct(LocalRef local) {
		return { .flag = OperationFlag::Flag::Construct, .local = local };
	}

	/**
	 * @brief Creates destruct flag for given local.
	 * @note: It is a function, not a constructor to avoid .hpp bloat.
	 * @param local
	 * @return constexpr OperationFlag
	 */
	constexpr OperationFlag flagDestruct(LocalRef local) {
		return { .flag = OperationFlag::Flag::Destruct, .local = local };
	}

	/**
	 * @brief Creates move flag for given local.
	 * @note: It is a function, not a constructor to avoid .hpp bloat.
	 * @param local
	 * @return constexpr OperationFlag
	 */
	constexpr OperationFlag flagMove(LocalRef local) {
		return { .flag = OperationFlag::Flag::Move, .local = local };
	}

	/**
	 * @brief Structure representing block in build process.
	 * @note It is a builder in the sense of design pattern.
	 */
	struct BlockBuilder final {
	private:
		BlockID id;

		/**
		 * @brief List of instructions kept in revered order.
		 * If given position does not have a value that means it is empty.
		 */
		std::vector<base::Optional<Instruction>> reversed_instruction;
		base::Optional<Instruction>              terminator;

		/**
		 * @brief Structure representing a hole in the block, that is
		 * empty instruction that has to be filled, before the block will be builded.
		 */
		struct InstructionHole final {
		private:
			BlockBuilderRef block_ref;
			usize           position;

			[[nodiscard]]
			bool isEmpty() const {
				return block_ref->reversed_instruction.at(position).empty();
			}

		public:
			InstructionHole(BlockBuilderRef block_ref, usize position):
				  block_ref(block_ref),
				  position(position) {}

			void fill(Instruction instruction) {
				CORE_ASSERT(isEmpty(), "Hole is already filled");
				CORE_ASSERT(
					not isTerminating(instruction.operation),
					"Instruction must not be a terminating instruction"
				);
				block_ref->reversed_instruction.at(position).emplace(std::move(instruction));
			}
		};

	public:
		BlockBuilder(usize vector_index): id(vector_index) {}

		[[nodiscard]]
		Block build() const {
			std::vector<Instruction> instructions;
			for (const auto& instruction: reversed_instruction | std::views::reverse) {
				CORE_ASSERT(instruction.has_value(), "Empty instruction left in the block");
				instructions.emplace_back(instruction.value());
			}
			return {
				.id           = id,
				.instructions = std::move(instructions),
				.terminator   = terminator.value(),
			};
		}

		/**
		 * @brief Adds instruction to the block.
		 * @note Instructions are added from last to first
		 * @param instr
		 */
		void addInstruction(Instruction instr) {
			CORE_ASSERT(
				not isTerminating(instr.operation),
				"Instruction must not be a terminating instruction"
			);
			reversed_instruction.emplace_back(std::move(instr));
		}

		/**
		 * @brief Adds instruction hole, that can be filled later.
		 * @note It is needed when one does not know the instruction he has to add, before something
		 * else will be lowered.
		 * @return InstructionHole
		 */
		InstructionHole addHole() {
			// this emplaces empty optional:
			reversed_instruction.emplace_back();

			// creation of borrow pointer here, depends on the fact that blocks
			// are kept in stable container:
			return { this, reversed_instruction.size() - 1 };
		}

		void setTerminator(Instruction instruction) {
			CORE_ASSERT(not terminator.has_value(), "terminator already set.");
			CORE_ASSERT(
				isTerminating(instruction.operation), "Terminator must be a terminating instruction"
			);
			terminator.emplace(std::move(instruction));
		}

		[[nodiscard]]
		BlockID getID() const {
			return id;
		}
	};

	/**
	 * @brief Structure representing function in build process.
	 * @note It is a builder in the sense of design pattern.
	 */
	struct FunctionBuilder final {
	private:
		base::Optional<base::StrID>      name;
		base::StableVector<BlockBuilder> blocks;
		base::Optional<BlockBuilderRef>  entry_block;
		base::StableVector<MirLocal>     local_list;
		base::Optional<helios::ScopeID>  top_lifetime_scope;

		query::Context& ctx;
		helios::SymID   helios_symbol;

	public:
		FunctionBuilder(query::Context& ctx, const helios::SymID helios_symbol):
			  ctx(ctx),
			  helios_symbol(helios_symbol) {}

		[[nodiscard]]
		Function build() {
			CORE_ASSERT(entry_block.has_value(), "Entry block not set");
			auto entry_block_id = entry_block.value()->getID();

			std::vector<BlockID> block_order;
			block_order.reserve(this->blocks.size());

			base::StableHashMap<BlockID, Block> function_blocks;
			
			// First element in block order is the entry block
			block_order.push_back(entry_block_id);

			// Count in "reverse order" to have more intuitive order
			// since creation of blocks is done from the end of the function.
			for (usize i = this->blocks.size(); i-- > 0; ) {
				Block block = this->blocks.getRef(i).value()->build();
				
				if (block.id != entry_block_id) // entry block is already added to the block order
					block_order.emplace_back(block.id);

				function_blocks.put(block.id, std::move(block));
			}

			const auto function_type
				= tsh::SymbolType<tsh::FunctionAbstractType>(
					  ctx.query<helios::QueryTypeOfSymbol>(helios_symbol)
						  ->expect("Handling errors in HOUT is not supported yet")
				)
			          .getType();

			return Function{
				name.value(),
				function_type.getResultType(),
				function_type.getParameterTypes(),
				std::move(function_blocks),
				std::move(block_order),
				std::move(local_list),
				top_lifetime_scope.value(),
				helios_symbol,
			};
		}

		void setName(base::StrID name) {
			CORE_ASSERT(not this->name.has_value(), "Name already set");
			this->name.emplace(name);
		}

		void setTopLifetimeScope(helios::ScopeID scope) {
			CORE_ASSERT(top_lifetime_scope.empty(), "Top lifetime scope already set");
			top_lifetime_scope.emplace(scope);
		}

		/**
		 * Adds a local variable to MIR function, from helios_id representing it.
		 */
		LocalRef addLocal(const helios::SymID helios_id) {
			const auto key = local_list.emplaceBack(MirLocal{
				helios_id,
				ctx.query<helios::QueryTypeOfSymbol>(helios_id)->expect(
					"Handling ERRORS in MIR is not supported yet..."
				),
				scope(helios_id),
			});
			return local_list.getRef(key).value();
		}

		/**
		 * Adds a local parameter variable to MIR function from helios_id representing it.
		 */
		LocalRef addParameter(const helios::SymID helios_id, u64 parameter_index) {
			CORE_ASSERT(kind(helios_id) == helios::SymbolKind::Parameter, "Not a parameter");
			const auto key = local_list.emplaceBack(MirLocal{
				helios_id,
				ctx.query<helios::QueryTypeOfSymbol>(helios_id)->expect(
					"Handling ERRORS in MIR is not supported yet..."
				),
				scope(helios_id),
				parameter_index,
			});
			return local_list.getRef(key).value();
		}

		[[nodiscard]]
		LocalRef addTmp(const tsh::SymbolType<> type, const helios::ScopeID scope) {
			const auto key = local_list.emplaceBack(MirLocal{ type, scope });
			return local_list.getRef(key).value();
		}

		/**
		 * Finds the location of a local variable in the function. Does not check the global scope.
		 * @param helios_id The HELIoS symbol ID of the local variable.
		 * @return The local variable reference, if found.
		 */
		[[nodiscard]]
		LocalRef findLocal(const helios::SymID helios_id) const {
			// @TODO: Optimise into a hashmap.
			for (const auto& local: local_list)
				if (local->helios_id == helios_id) return local.ref();
			CORE_PANIC(base::strConcat("MIR Local not found: ", compiler::helios::name(helios_id)));
		}

		[[nodiscard]]
		BlockBuilderRef newBlock() {
			auto index = blocks.size();
			auto res   = blocks.getRef(blocks.emplaceBack(BlockBuilder{ index })).value();
			CORE_ASSERT(u64(res->getID()) == blocks.size() - 1, "Bad block id");
			return res;
		}

		void setEntry(BlockBuilderRef block) {
			CORE_ASSERT(entry_block.empty(), "Entry block already set.");
			entry_block.emplace(block);
		}

		query::Context& getContext() { return ctx; }

		/**
		 * This is needed only for some assertins.
		 */
		[[nodiscard]]
		helios::SymID getHeliosSymbol() const {
			return helios_symbol;
		}
	};

	/**
	 * @brief Visitor that collects all local variables in the function and adds them directly to
	 * the FunctionBuilder.
	 */
	struct LocalVarCollectionVisitor: public hc::HoutStmtVisitorPanicky {
		FunctionBuilder& function;

		LocalVarCollectionVisitor(FunctionBuilder& function): function(function) {}

		/**
		 * Helper function that recursively goes over the code block and collects all local
		 * variables.
		 */
		void goOverCodeBlock(const hc::CodeBlock& code_block) {
			for (const auto& stmt: code_block.statements) stmt->acceptVisitor(*this);
		}

		/**
		 * @brief Collects all local variables in the function and adds them directly to the
		 * FunctionBuilder.
		 */
		void collect(const helios::HOUTFunction& hout_function) {
			CORE_ASSERT(
				hout_function.original_symbol == function.getHeliosSymbol(),
				"Bad function passed to LocalVarCollectionVisitor"
			);

			u64 parameter_index = 0;
			for (const auto& parameter: *hout_function.content.parameters) {
				function.addParameter(parameter.helios_symbol, parameter_index);
				parameter_index++;
			}
			goOverCodeBlock(*hout_function.content.body);
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			function.addLocal(stmt.helios_symbol);
		}

		void visitIfStmt(const helios::code::IfStmt& stmt) override { goOverCodeBlock(stmt.body); }

		// Explicit empty boilerplate. Expected changes when block expressions are implemented.

		void visitReturnStmt(const helios::code::ReturnStmt&) override {}

		void visitVoidReturnStmt(const helios::code::VoidReturnStmt&) override {}

		void visitExprStmt(const helios::code::ExprStmt&) override {}

		void visitAssignmentStmt(const helios::code::AssignmentStmt&) override {}
	};

	/**
	 * @brief Visitor that implements actual logic of lowering statements.
	 * @note The result of the visitor is stored in out member.
	 */
	struct StmtBlockVisitor: public hc::HoutStmtVisitor {
		BlockBuilderRef continuation;

		FunctionBuilder& function;

		StmtBlockVisitor(BlockBuilderRef continuation, FunctionBuilder& function):
			  continuation(continuation),
			  function(function) {}

		base::Optional<StmtLowerRes> out;

		void output(StmtLowerRes value) {
			CORE_ASSERT(this->out.empty(), "Output already set.");
			this->out.emplace(value);
		}

		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			auto return_block = function.newBlock();

			// lower expr:
			auto expr_res = lowerExpr(*stmt.value, return_block, function);

			return_block->setTerminator(
				Instruction(Operation::ReturnValue, {}, { expr_res.value }, {}, stmt.lifetime_scope)
			);

			output({ expr_res.begin });
		}

		void visitVoidReturnStmt(const hc::VoidReturnStmt& stmt) override {
			auto return_block = function.newBlock();
			return_block->setTerminator({ Operation::ReturnVoid, {}, {}, {}, stmt.lifetime_scope });
			output({ return_block });
		}

		void visitExprStmt(const hc::ExprStmt& stmt) override {
			auto expr_result = lowerExpr(*stmt.expr, continuation, function);

			output({ expr_result.begin });
		}

		void visitIfStmt(const hc::IfStmt& stmt) override {
			// @TODO: else body
			auto else_block = function.newBlock();
			else_block->setTerminator(
				{ Operation::Jump, {}, { continuation->getID() }, {}, stmt.lifetime_scope }
			);

			// The "then" branch requires a new block,
			// because otherwise the "else" branch would jump to it.
			auto then_block = function.newBlock();
			then_block->setTerminator(
				{ Operation::Jump, {}, { continuation->getID() }, {}, stmt.lifetime_scope }
			);
			auto then_body = lowerCodeBlock(stmt.body, then_block, function);

			// @TODO: Implement jumpy code here.
			auto condition_block = function.newBlock();
			auto expr_result     = lowerExpr(*stmt.condition, condition_block, function);

			condition_block->setTerminator(
				{ Operation::Branch,
			      {},
			      { expr_result.value, then_body.begin->getID(), else_block->getID() },
			      {},
			      stmt.lifetime_scope }
			);

			output({ expr_result.begin });
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			auto local                   = function.findLocal(stmt.helios_symbol);
			auto local_construction_hole = continuation->addHole();

			match_optional(stmt.initial_value) {
				opt_some(value) {
					auto expr_result = lowerExpr(*value, continuation, function);

					local_construction_hole.fill(Instruction{
						Operation::Assign,
						{ local },
						{ expr_result.value },
						{ flagConstruct(local) },
						stmt.lifetime_scope,
					});

					output({ expr_result.begin });
					return;
				}
				opt_none { throw base::NotYetImplemented("variable without initial value in MIR"); }
			}

			CORE_UNREACHABLE();
		}

		void visitAssignmentStmt(const helios::code::AssignmentStmt& stmt) override {
			// TODO: #448 Search for location in global scope as well.
			// TODO: #469 Support arbitrary lvalues on the left.
			auto target_location            = function.findLocal(stmt.helios_symbol);
			auto target_construction_hole   = continuation->addHole();
			auto [sub_continuation, result] = lowerExpr(*stmt.new_value, continuation, function);

			target_construction_hole.fill(Instruction{
				Operation::Assign,
				{ target_location },
				{ result },
				{},
				stmt.lifetime_scope,
			});

			output({ sub_continuation });
		}
	};

	/**
	 * @brief Visitor that implements actual logic of lowering expression.
	 * @note The result of the visitor is stored in out member.
	 */
	struct ExprBlockVisitor: public hc::HoutExprVisitor {
		BlockBuilderRef continuation;

		base::Optional<ExprLowerRes> out;

		FunctionBuilder& function;

		ExprBlockVisitor(BlockBuilderRef continuation, FunctionBuilder& function):
			  continuation(continuation),
			  function(function) {}

		void output(ExprLowerRes value) {
			CORE_ASSERT(this->out.empty(), "Output already set.");
			this->out.emplace(value);
		}

		void visitLiteralIntExpr(const hc::LiteralIntExpr& expr) override {
			output({ .begin = continuation, .value = MIRValue{ MirIntegerConst{ expr.value } } });
		}

		void visitLiteralBoolExpr(const hc::LiteralBoolExpr& expr) override {
			output({ .begin = continuation, .value = MIRValue{ MirBoolConst{ expr.value } } });
		}

		void visitLiteralTypeExpr(const hc::LiteralTypeExpr&) override {
			throw base::NotYetImplemented("type literal");
		}

		void visitIdentifierExpr(const hc::IdentifierExpr& expr) override {
			output({ .begin = continuation,
			         .value = MIRValue{ function.findLocal(expr.symbol).get() } });
		}

		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole          = continuation->addHole();
			const auto [r_continuation, right_res] = lowerExpr(*expr.rhs, continuation, function);
			const auto [l_continuation, left_res]  = lowerExpr(*expr.lhs, r_continuation, function);

			// Fill the hole with the binary operation.
			// Assume (for now?) that the arguments are of the same type,
			// and the result is of the same type as the arguments.
			const auto argument_type       = locationType(right_res, function.getContext());
			const auto other_argument_type = locationType(left_res, function.getContext());
			CORE_ASSERT(
				argument_type.getType() == other_argument_type.getType(),
				"Binary operator with different types"
			);
			const auto      target_location = function.addTmp(argument_type, expr.lifetime_scope);
			const Operation operation       = builtinBinaryToOperation(expr.operation);
			target_construction_hole.fill(Instruction{
				operation,
				{ target_location },
				{ left_res, right_res },
				{ flagConstruct(target_location) },
				expr.lifetime_scope,
			});
			output({ .begin = l_continuation, .value = target_location });
		}

		void visitUnaryOperatorExpr(const hc::UnaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole          = continuation->addHole();
			const auto [sub_continuation, sub_res] = lowerExpr(*expr.expr, continuation, function);

			// Fill the hole with the unary operation.
			const auto      argument_type   = locationType(sub_res, function.getContext());
			const auto      target_location = function.addTmp(argument_type, expr.lifetime_scope);
			const Operation operation       = builtinUnaryToOperation(expr.operation);
			target_construction_hole.fill(Instruction{
				operation,
				{ target_location },
				{ sub_res },
				{ flagConstruct(target_location) },
				expr.lifetime_scope,
			});

			output({ .begin = sub_continuation, .value = target_location });
		}

		void visitParenthesisExpr(const hc::ParenthesisExpr& expr) override {
			output(lowerExpr(*expr.inner, continuation, function));
		}

		void visitTupleTypeConstructorExpr(const hc::TupleTypeConstructorExpr&) override {
			throw base::NotYetImplemented("tuple constructor");
		}

		void visitVariantTypeConstructorExpr(const hc::VariantTypeConstructorExpr&) override {
			throw base::NotYetImplemented("variant constructor");
		}

		void visitLinkedIdentifierExpr(const hc::LinkedIdentifierExpr&) override {
			throw base::NotYetImplemented("linked identifier expr");
		}

		void visitCallExpr(const hc::CallExpr& expr) override {
			auto       call = continuation->addHole();
			const auto call_result
				= function.addTmp(expr.expression_type.getSymbolType(), expr.lifetime_scope);

			auto                  sub_continuation = continuation;
			std::vector<MIRValue> args;
			args.emplace_back(MirFunctionLiteral{ expr.callee });
			for (const auto& arg: expr.arguments) {
				auto [expr_continuation, sub_res] = lowerExpr(*arg, sub_continuation, function);
				args.push_back(sub_res);
				sub_continuation = expr_continuation;
			}

			// @TODO: #505 here in the future we (probably) will have to handle
			// move operations related to the passing of the arguments to the function

			call.fill(Instruction{
				Operation::Call,
				{ call_result },
				args,
				{ flagConstruct(call_result) },
				expr.lifetime_scope,
			});


			return output({ .begin = sub_continuation, .value = call_result });
		}

	private:
		static Operation builtinBinaryToOperation(const helios::code::BuiltinBinary builtin) {
			using enum helios::code::BuiltinBinary;
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
			default:
				CORE_UNREACHABLE();
			}
		}

		static Operation builtinUnaryToOperation(const helios::code::BuiltinUnary builtin) {
			using enum helios::code::BuiltinUnary;
			switch (builtin) {
			case IntegerNegation:
				return Operation::IntegerNeg;
			default:
				CORE_UNREACHABLE();
			}
		}

		/**
		 * Get the type of a location, assuming that it is a local value.
		 * @param location A MIR location which holds a local value.
		 * @param ctx The query context for AbstractType generation.
		 * @return The type of the local value.
		 */
		static tsh::SymbolType<> locationType(const MIRValue location, query::Context& ctx) {
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
				variant_default { CORE_UNREACHABLE(); }
			}
			CORE_UNREACHABLE();
		}
	};

	StmtLowerRes
		lowerStmt(const hc::Stmt& stmt, BlockBuilderRef continuation, FunctionBuilder& function) {
		StmtBlockVisitor visitor{ continuation, function };
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes
		lowerExpr(const hc::Expr& expr, BlockBuilderRef continuation, FunctionBuilder& function) {
		ExprBlockVisitor visitor{ continuation, function };
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block, BlockBuilderRef continuation, FunctionBuilder& function
	) {
		StmtLowerRes last_result{ continuation };
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result  = lowerStmt(*stmt, continuation, function);
			continuation = last_result.begin;
		}
		return last_result;
	}

	// @TODO: StmtExprBoolJmpVisitor for jumping code

	Function lowerToPreMirFunction(query::Context& ctx, const helios::HOUTFunction& function) {
		FunctionBuilder function_builder{ ctx, function.original_symbol };
		function_builder.setName(function.original_name);
		function_builder.setTopLifetimeScope(function.top_lifetime_scope);

		LocalVarCollectionVisitor visitor{ function_builder };
		visitor.collect(function);

		auto fun_body_scope = function.content.body->lifetime_scope;
		auto last_block     = function_builder.newBlock();
		last_block->setTerminator({ Operation::FunctionEnd, {}, {}, {}, fun_body_scope });

		// build cfg+quad step by step:
		auto first_block = lowerCodeBlock(*function.content.body, last_block, function_builder);

		function_builder.setEntry(first_block.begin);

		return function_builder.build();
	}

	struct IMPLEMENT_QUERY(LowerToMirFunction, Function) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// first step: lowering to pre-mir (cfg+quad)
			auto function_no_lifetime = lowerToPreMirFunction(ctx, key.function);

			// second step: lifetime stuff
			return addDestructors(ctx, std::move(function_no_lifetime));
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);
}
