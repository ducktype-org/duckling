/**
 * @file mir_lowering.cpp
 * @brief Implementation of lowering HOUT functions to MIR functions.
 * The creation of MIR is done "in reverse" that is from function end to its beginning.
 */

#include "mir_lowering.hpp"
#include <query_framework/query_impl.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
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
	 * a BlockBuilderRef that is the beginning of the lowered expression and MirLocation
	 * that holds the result of the expression.
	 */
	struct ExprLowerRes final {
		BlockBuilderRef begin;
		MirLocation     value;
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
		return { OperationFlag::Flag::Construct, local };
	}

	/**
	 * @brief Creates destruct flag for given local.
	 * @note: It is a function, not a constructor to avoid .hpp bloat.
	 * @param local
	 * @return constexpr OperationFlag
	 */
	constexpr OperationFlag flagDestruct(LocalRef local) {
		return { OperationFlag::Flag::Destruct, local };
	}

	/**
	 * @brief Creates move flag for given local.
	 * @note: It is a function, not a constructor to avoid .hpp bloat.
	 * @param local
	 * @return constexpr OperationFlag
	 */
	constexpr OperationFlag flagMove(LocalRef local) {
		return { OperationFlag::Flag::Move, local };
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

		public:
			InstructionHole(BlockBuilderRef block_ref, usize position):
				  block_ref(block_ref),
				  position(position) {}

			void fill(Instruction instruction) {
				RIFT_ASSERT(
					block_ref->reversed_instruction.at(position).empty(), "Hole is already filled"
				);
				RIFT_ASSERT(
					not isTerminating(instruction.operation),
					"Instruction must not be a terminating instruction"
				);
				block_ref->reversed_instruction.at(position).emplace(std::move(instruction));
			}
		};

	public:
		BlockBuilder(usize vector_index): id(vector_index){};

		[[nodiscard]]
		Block build() const {
			std::vector<Instruction> instructions;
			for (const auto& instruction: reversed_instruction | std::views::reverse) {
				RIFT_ASSERT(instruction.has_value(), "Empty instruction left in the block");
				instructions.emplace_back(instruction.value());
			}
			return { id, std::move(instructions), terminator.value() };
		}

		/**
		 * @brief Adds instruction to the block.
		 * @note Instructions are added from last to first
		 * @param instr
		 */
		void addInstruction(Instruction instr) {
			RIFT_ASSERT(
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
			RIFT_ASSERT(not terminator.has_value(), "terminator already set.");
			RIFT_ASSERT(
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

	public:
		Function build() {
			RIFT_ASSERT(entry_block.has_value(), "Entry block not set");

			std::vector<Block> blocks;
			for (usize i = 0; i < this->blocks.size(); i++)
				blocks.emplace_back(this->blocks.getRef(i).value()->build());

			return Function{
				name.value(), std::move(blocks), std::move(local_list), entry_block.value()->getID()
			};
		}

		void setName(base::StrID name) {
			RIFT_ASSERT(not this->name.has_value(), "Name already set");
			this->name.emplace(name);
		}

		[[nodiscard]]
		LocalRef addLocal(helios::SymID helios_id) {
			auto key = local_list.emplaceBack(MirLocal{ helios_id });
			return local_list.getRef(key).value();
		}

		[[nodiscard]]
		BlockBuilderRef newBlock() {
			auto index = blocks.size();
			auto res   = blocks.getRef(blocks.emplaceBack(BlockBuilder{ index })).value();
			RIFT_ASSERT(u64(res->getID()) == blocks.size() - 1, "Bad block id");
			return res;
		}

		void setEntry(BlockBuilderRef block) {
			RIFT_ASSERT(entry_block.empty(), "Entry block already set.");
			entry_block.emplace(block);
		}
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
			RIFT_ASSERT(this->out.empty(), "Output already set.");
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

			// @note: here then block created a new block
			// so "else" will not jump into it.
			// @future: the current solution may be sub-optimal
			// we will have to look into it.
			auto then_block = function.newBlock();
			then_block->setTerminator(
				{ Operation::Jump, {}, { continuation->getID() }, {}, stmt.lifetime_scope }
			);
			auto then_body = lowerCodeBlock(stmt.body, then_block, function);

			// @future: in the future we wan't jumpy code here
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
			auto local                   = function.addLocal(stmt.helios_symbol);
			auto local_construction_hole = continuation->addHole();

			match_optional(stmt.initial_value) {
				opt_some(value) {
					auto expr_result = lowerExpr(*value, continuation, function);

					local_construction_hole.fill(Instruction{ Operation::Assign,
					                                          { local },
					                                          { expr_result.value },
					                                          { flagConstruct(local) },
					                                          stmt.lifetime_scope });

					output({ expr_result.begin });
					return;
				}
				opt_none { throw base::NotYetImplemented("variable without initial value in MIR"); }
			}

			RIFT_PANIC("match_optional failed in visitVariableStmt.");
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
			RIFT_ASSERT(this->out.empty(), "Output already set.");
			this->out.emplace(value);
		}

		void visitLiteralValueExpr(const hc::LiteralValueExpr& expr) override {
			output({ continuation, MirLocation{ MirIntegerConst{ expr.value } } });
		}

		void visitIdentifierExpr(const hc::IdentifierExpr&) override {
			throw base::NotYetImplemented("identifier");
		}

		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr&) override {
			throw base::NotYetImplemented("binary operator");
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

	struct IMPLEMENT_QUERY(LowerToMirFunction, Function) {
		static auto provide(Context&, QKey key) -> PResult {
			// First step:
			// * build cfg+quad step by step


			FunctionBuilder function_builder;

			function_builder.setName(key.function.original_name);

			// @TODO: add parameters stuff

			auto fun_body_scope = key.function.body.body->lifetime_scope;

			auto last_block = function_builder.newBlock();
			last_block->setTerminator({ Operation::FunctionEnd, {}, {}, {}, fun_body_scope });

			auto first_block
				= lowerCodeBlock(*key.function.body.body, last_block, function_builder);

			function_builder.setEntry(first_block.begin);

			// second step: lifetime stuff

			// @TODO:
			// * add some lifetime stuff

			return function_builder.build();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);


}
