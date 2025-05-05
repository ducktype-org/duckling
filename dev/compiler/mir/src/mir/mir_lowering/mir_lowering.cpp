/**
 * @file mir_lowering.cpp
 * @brief Implementation of lowering HOUT functions to MIR functions.
 * The creation of MIR is done "in reverse" that is from function end to its beginning.
 */

#include "mir_lowering.hpp"

#include "mir_lifetimes.hpp"

#include <helios/helios_result.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>
#include <base/variant.hpp>

#include <stack>
#include <unordered_set>

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
	 * @param parent_scope Scope of the parent of this Statement.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerStmt(
		const hc::Stmt& stmt, BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope
	);

	/**
	 * @brief Lowers expression.
	 *
	 * @param expr
	 * @param continuation Block that should be executed after this expression.
	 * @param function Function that we are lowering this expression in.
	 * @param expr_scope Lifetime Scope this expression should be in.
	 * @return ExprLowerRes
	 */
	ExprLowerRes lowerExpr(
		const hc::Expr& expr, BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope
	);

	/**
	 * @brief Lowers code-block, by lowering all statements in the block.
	 *
	 * @param code_block
	 * @param continuation Block that should be executed after this code block.
	 * @param function Function that we are lowering this code block in.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block, BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope
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

		LifetimeScopeTree lifetime_scope_tree;

		/**
		 * @brief Top level scope of the function.
		 * it is different from the root scope of litetime tree,
		 * since the root scope is the scope in which nothing
		 * should live.
		 * @important: This has to be defined bellow lifetime_scope_tree,
		 * since lifetime_scope_tree is used in its initialization.
		 */
		ScopeRef top_level_scope;

		query::Context& ctx;
		helios::SymID   helios_symbol;

	public:
		FunctionBuilder(query::Context& ctx, const helios::SymID helios_symbol):
			  top_level_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
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
			for (usize i = this->blocks.size(); i-- > 0;) {
				Block block = this->blocks[i]->build();
				function_blocks.put(block.id, std::move(block));

				if (block.id != entry_block_id)  // entry block is already added to the block_order
					block_order.emplace_back(block.id);
			}

			const auto function_type
				= tsh::SymbolType<tsh::FunctionAbstractType>(
					  ctx.query<helios::QueryTypeOfSymbol>(helios_symbol)
						  ->expect("Handling errors in HOUT is not supported yet")
				)
			          .getType();

			// we sanity check here, that all local variables
			// that have a helios id also have lifetime scope,
			// as thery always have to have it.
			for (auto& local: local_list) {
				if (local->helios_id.has_value()) {
					CORE_ASSERT(
						local->scope.has_value(),
						"Local variable without lifetime scope"
					);
				}
			}

			return Function{
				name.value(),
				function_type.getResultType(),
				function_type.getParameterTypes(),
				std::move(function_blocks),
				std::move(block_order),
				std::move(local_list).toConstData(),
				std::move(lifetime_scope_tree),
				helios_symbol,
			};
		}

		void setName(base::StrID name) {
			CORE_ASSERT(not this->name.has_value(), "Name already set");
			this->name.emplace(name);
		}

		/**
		 * Adds a local variable to MIR function, from helios_id representing it.
		 */
		MutLocalRef addLocal(const helios::SymID helios_id) {
			local_list.emplaceBack(MirLocal{
				helios_id,
				ctx.query<helios::QueryTypeOfSymbol>(helios_id)->expect(
					"Handling ERRORS in MIR is not supported yet..."
				),
			});
			return local_list.last();
		}

		/**
		 * Adds a local parameter variable to MIR function from helios_id representing it.
		 */
		MutLocalRef addParameter(const helios::SymID helios_id, u64 parameter_index) {
			CORE_ASSERT(kind(helios_id) == helios::SymbolKind::Parameter, "Not a parameter");
			local_list.emplaceBack(MirLocal{
				helios_id,
				ctx.query<helios::QueryTypeOfSymbol>(helios_id)->expect(
					"Handling ERRORS in MIR is not supported yet..."
				),
				parameter_index,
			});
			return local_list.last();
		}

		/**
		 * Creates a temporary local value, i.e. local value
		 * not arising from variable written directly in the Duckling source code.
		 */
		[[nodiscard]]
		MutLocalRef addTmp(const tsh::SymbolType<> type) {
			local_list.emplaceBack(MirLocal{ type });
			return local_list.last();
		}

		/**
		 * Creates a temporary local value, and also sets its lifetime scope.
		 */
		 [[nodiscard]]
		 MutLocalRef addTmp(const tsh::SymbolType<> type, ScopeRef scope) {
			 auto tmp = addTmp(type);
			 tmp->setLifetimeScope(scope);
			 return tmp;
		 }
		
		/**
		 * Add a temporary value of type bool.
		 * Used for example by if/while lowering to store
		 * the result of the condition.
		 */
		[[nodiscard]]
		MutLocalRef addBoolTmp() {
			auto type = tsh::SymbolType<>(ctx.query<tsh::QueryBoolType>({}), tsh::ReferenceKind::Direct, tsh::Mutability::Immutable);
			local_list.emplaceBack(MirLocal{ type });
			return local_list.last();
		}

		/**
		 * Finds the location of a local variable in the function. Does not check the global scope.
		 * @param helios_id The HELIoS symbol ID of the local variable.
		 * @return The local variable reference, if found.
		 */
		[[nodiscard]]
		MutLocalRef findLocal(const helios::SymID helios_id) const {
			// @TODO: Optimize into a hashmap.
			for (const auto& local: local_list)
				if (local->helios_id == helios_id) return local.refMut();
			CORE_PANIC(base::strConcat("MIR Local not found: ", compiler::helios::name(helios_id)));
		}

		[[nodiscard]]
		BlockBuilderRef newBlock() {
			auto vector_index = blocks.size();
			blocks.emplaceBack(BlockBuilder{ vector_index });
			CORE_ASSERT(u64(blocks.last()->getID()) == blocks.lastIndex(), "Bad block id");
			return blocks.last();
		}

		void setEntry(BlockBuilderRef block) {
			CORE_ASSERT(entry_block.empty(), "Entry block already set.");
			entry_block.emplace(block);
		}

		[[nodiscard]]
		auto getTopLevelScope() const {
			return top_level_scope;
		}

		[[nodiscard]]
		auto newScope(ScopeRef parent) {
			return lifetime_scope_tree.newScope(parent);
		}

		[[nodiscard]]
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
	 * It sets variable scopes for parameters, but doesn't set it for other local variables.
	 * Scope of other local variables is set when visiting VariableStmt in StmtBlockVisitor,
	 * since only then is the scope of the variable known.
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
				auto local = function.addParameter(parameter.helios_symbol, parameter_index);
				local->setLifetimeScope(function.getTopLevelScope());
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

		/**
		 * Scope of the parent.
		 */
		ScopeRef parent_scope;

		StmtBlockVisitor(BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope):
			  continuation(continuation),
			  function(function),
			  parent_scope(parent_scope) {}

		base::Optional<StmtLowerRes> out;

		void output(StmtLowerRes value) {
			CORE_ASSERT(this->out.empty(), "Output already set.");
			this->out.emplace(value);
		}

		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			auto return_block = function.newBlock();
			auto return_scope = function.newScope(parent_scope);

			auto retrieve_value = return_block->addHole();

			// lower expr:
			auto expr_res = lowerExpr(*stmt.value, return_block, function, return_scope);

			if (expr_res.value.isLocal()) {
				// we need to store the result of the expression
				// in additional variable, so it doesn't get destroyed:
				auto return_value = function.addTmp(expr_res.value.get<LocalRef>()->type);
				retrieve_value.fill(Instruction{
					Operation::Assign,
					{ return_value },
					{ expr_res.value },
					{ flagConstruct(return_value), flagMove(expr_res.value.get<LocalRef>()) },
					return_scope,
				});
				return_block->setTerminator(
					Instruction(Operation::ReturnValue, {}, { return_value }, {}, return_scope)
				);
			}
			else {	
				retrieve_value.fill(Instruction{
					Operation::Nop,
					{ },
					{ },
					{ },
					return_scope,
				});
				return_block->setTerminator(
					Instruction(Operation::ReturnValue, {}, { expr_res.value }, {}, return_scope)
				);
			}

			output({ expr_res.begin });
		}

		void visitVoidReturnStmt(const hc::VoidReturnStmt&) override {
			auto return_block = function.newBlock();
			auto return_scope = function.newScope(parent_scope);
			return_block->setTerminator({ Operation::ReturnVoid, {}, {}, {}, return_scope });
			output({ return_block });
		}

		void visitExprStmt(const hc::ExprStmt& stmt) override {
			auto expr_scope = function.newScope(parent_scope);
			auto expr_result = lowerExpr(*stmt.expr, continuation, function, expr_scope);

			output({ expr_result.begin });
		}

		void visitIfStmt(const hc::IfStmt& stmt) override {

			auto condition_scope = function.newScope(parent_scope);

			// I'm not sure if we need these scopes,
			// maybe we could just pass parent_scope as-is.
			// But this way it for sure works.
			auto then_scope = function.newScope(parent_scope);
			auto else_scope = function.newScope(parent_scope);

			// @TODO: else body
			auto else_block = function.newBlock();
			else_block->setTerminator(
				{ Operation::Jump, {}, { continuation->getID() }, {}, else_scope }
			);

			// The "then" branch requires a new block,
			// because otherwise the "else" branch would jump to it.
			auto then_block = function.newBlock();
			then_block->setTerminator(
				{ Operation::Jump, {}, { continuation->getID() }, {}, then_scope }
			);
			auto then_body = lowerCodeBlock(stmt.body, then_block, function, then_scope);

			// @TODO: Implement jumpy code here.
			auto condition_block = function.newBlock();
			
			auto get_condition_return = condition_block->addHole();

			auto expr_result     = lowerExpr(*stmt.condition, condition_block, function, condition_scope);

			if (expr_result.value.isLocal()) {
				// we have to "move" the condition result
				// into special temporary value, so we can use it
				// after the actual condition result is destroyed.
				auto condition_result_tmp = function.addBoolTmp();

				get_condition_return.fill(Instruction{
					Operation::Assign,
					{ condition_result_tmp },
					{ expr_result.value },
					{ flagConstruct(condition_result_tmp) },
					condition_scope,
				});

				condition_block->setTerminator(
					{ 
						Operation::Branch,
						{},
						{condition_result_tmp, then_body.begin->getID(), else_block->getID() },
						{},
						condition_scope,
					}
				);

			}
			else {
				// we can use the result of the expression directly:

				get_condition_return.fill(Instruction{
					Operation::Nop,
					{ },
					{ },
					{ },
					condition_scope,
				});

				condition_block->setTerminator(
					{ 
						Operation::Branch,
						{},
						{expr_result.value, then_body.begin->getID(), else_block->getID() },
						{},
						condition_scope,
					}
				);
			}
			
			output({ expr_result.begin });
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			auto local                   = function.findLocal(stmt.helios_symbol);
			
			// we set the lifetime scope of the local variable here
			// since we only know it here:
			local->setLifetimeScope(parent_scope);

			auto local_construction_hole = continuation->addHole();

			match_optional(stmt.initial_value) {
				opt_some(value) {
					auto assignment_scope = function.newScope(parent_scope);
					auto expr_result = lowerExpr(*value, continuation, function, assignment_scope);

					local_construction_hole.fill(Instruction{
						Operation::Assign,
						{ local },
						{ expr_result.value },
						{ flagConstruct(local) },
						assignment_scope,
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
			
			auto assignment_scope = function.newScope(parent_scope);		

			auto target_location            = function.findLocal(stmt.helios_symbol);
			auto target_construction_hole   = continuation->addHole();
			auto [sub_continuation, result] = lowerExpr(*stmt.new_value, continuation, function, assignment_scope);

			target_construction_hole.fill(Instruction{
				Operation::Assign,
				{ target_location },
				{ result },
				{},
				assignment_scope,
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

		/**
		 * The scope of the expression, where it and its result should live in.
		 */
		ScopeRef expr_scope;

		ExprBlockVisitor(BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope):
			  continuation(continuation),
			  function(function),
			  expr_scope(expr_scope) {}

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
			         .value = MIRValue{ LocalRef(function.findLocal(expr.symbol).get()) } });
		}

		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole          = continuation->addHole();
			const auto [r_continuation, right_res] = lowerExpr(*expr.rhs, continuation, function, expr_scope);
			const auto [l_continuation, left_res]  = lowerExpr(*expr.lhs, r_continuation, function, expr_scope);

			// Fill the hole with the binary operation.
			// Assume (for now?) that the arguments are of the same type,
			// and the result is of the same type as the arguments.
			const auto argument_type       = locationType(right_res, function.getContext());
			const auto other_argument_type = locationType(left_res, function.getContext());
			CORE_ASSERT(
				argument_type.getType() == other_argument_type.getType(),
				"Binary operator with different types"
			);
			const auto      target_location = function.addTmp(argument_type, expr_scope);
			const Operation operation       = builtinBinaryToOperation(expr.operation);
			target_construction_hole.fill(Instruction{
				operation,
				{ target_location },
				{ left_res, right_res },
				{ flagConstruct(target_location) },
				expr_scope,
			});
			output({ .begin = l_continuation, .value = target_location });
		}

		void visitUnaryOperatorExpr(const hc::UnaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole          = continuation->addHole();
			const auto [sub_continuation, sub_res] = lowerExpr(*expr.expr, continuation, function, expr_scope);

			// Fill the hole with the unary operation.
			const auto      argument_type   = locationType(sub_res, function.getContext());
			const auto      target_location = function.addTmp(argument_type, expr_scope);
			const Operation operation       = builtinUnaryToOperation(expr.operation);
			target_construction_hole.fill(Instruction{
				operation,
				{ target_location },
				{ sub_res },
				{ flagConstruct(target_location) },
				expr_scope,
			});

			output({ .begin = sub_continuation, .value = target_location });
		}

		void visitParenthesisExpr(const hc::ParenthesisExpr& expr) override {
			output(lowerExpr(*expr.inner, continuation, function, expr_scope));
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
				= function.addTmp(expr.expression_type.getSymbolType(), expr_scope);

			auto                  sub_continuation = continuation;
			std::vector<MIRValue> args;
			args.emplace_back(MirFunctionLiteral{ expr.callee });
			for (const auto& arg: expr.arguments) {
				auto [expr_continuation, sub_res] = lowerExpr(*arg, sub_continuation, function, expr_scope);
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
				expr_scope,
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

	StmtLowerRes lowerStmt(
		const hc::Stmt& stmt, BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope
	) {
		StmtBlockVisitor visitor{ continuation, function, parent_scope };
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes lowerExpr(
		const hc::Expr& expr, BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope
	) {
		ExprBlockVisitor visitor{ continuation, function, expr_scope };
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block, BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope
	) {
		StmtLowerRes last_result{ continuation };
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result  = lowerStmt(*stmt, continuation, function, parent_scope);
			continuation = last_result.begin;
		}
		return last_result;
	}

	// @TODO: StmtExprBoolJmpVisitor for jumping code

	Function lowerToPreMirFunction(query::Context& ctx, const helios::HOUTFunction& function) {
		FunctionBuilder function_builder{ ctx, function.original_symbol };
		function_builder.setName(function.original_name);

		LocalVarCollectionVisitor visitor{ function_builder };
		visitor.collect(function);

		auto last_block     = function_builder.newBlock();
		last_block->setTerminator({ Operation::FunctionEnd, {}, {}, {}, function_builder.getTopLevelScope() });

		// build cfg+quad step by step:
		auto first_block = lowerCodeBlock(*function.content.body, last_block, function_builder, function_builder.getTopLevelScope());

		function_builder.setEntry(first_block.begin);

		return function_builder.build();
	}

	/**
	 * @brief Deletes from mir Function (from block_order and blocks) unreachable blocks.
	 * Performs DFS on the CFG and marks every reachable block, then deletes the unreachable ones.
	 */
	Function eliminateUnreachable(Function function) {
		std::unordered_set<BlockID> reachable;
		std::stack<BlockID>         stack;

		stack.push(function.block_order[0]);
		while (!stack.empty()) {
			BlockID block_id = stack.top();
			stack.pop();

			if (reachable.contains(block_id)) continue;

			auto successors = getTerminatorSuccessors(function.blocks[block_id].terminator);

			reachable.insert(block_id);
			for (auto successor: successors) stack.push(successor);
		}

		std::vector<BlockID> new_block_order;

		for (auto block_id: function.block_order)
			if (reachable.contains(block_id))
				new_block_order.push_back(block_id);
			else
				function.blocks.erase(block_id);
		function.block_order = new_block_order;

		// WEAK_ASSERT candidate
		CORE_ASSERT(function.validateBlockIDs().isOk(), "Function has invalid block IDs");

		return function;
	}

	/**
	 * @brief Block with idx 0 of the MIR function has "FunctionEnd" terminator which is a mock-up.
	 *
	 * This function deals with this terminator:
	 * * if block doesn't exists it means that it was unreachable, we do nothing
	 * * if block is reachable, but function returns void it is replaced with ReturnVoid
	 * * if block is reachable and function returns value, throws missing return error

	 * @note It is assumed that the last block is the last in the block order.
	 */
	helios::errors::HResult<Function, helios::errors::Failed> finalizeFunctionEnd(
		query::Context&, Function function
	) {
		CORE_ASSERT(
			function.blocks.size() > 0, "Function should have at least one block after lowering"
		);

		// It should be always zero because the last block is generated as the first one.
		auto last_block_id = BlockID(0);
		if (not function.blocks.contains(last_block_id)) return function;

		CORE_ASSERT(
			function.blocks[last_block_id].terminator.operation == Operation::FunctionEnd,
			"Last block doesn't have FunctionEnd terminator"
		);

		if (function.return_type.getType().getKind() == tsh::Kind::Unit) {
			function.blocks[last_block_id].terminator.operation = Operation::ReturnVoid;
			return function;
		} else {
			// @todo there should be logging here of missing return value / control reaches the end
			// of non-void function
			return helios::errors::HError(helios::errors::Failed());
		}
	}

	struct IMPLEMENT_QUERY(LowerToMirFunction, LowerToMirFunctionResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// first step: lowering to pre-mir (cfg+quad)
			auto function_no_lifetime = lowerToPreMirFunction(ctx, key.function);

			// second step: lifetime stuff
			auto function_with_destructors = addDestructors(ctx, std::move(function_no_lifetime));

			// eliminating unreachable blocks
			auto function_reachable = eliminateUnreachable(std::move(function_with_destructors));

			// change FunctionEnd to proper return
			UNPACK_RESULT_MOVE(
				auto function_no_func_end =, finalizeFunctionEnd(ctx, std::move(function_reachable))
			);

			return function_no_func_end;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);
}
