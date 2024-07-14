#include "mir_lowering.hpp"
#include <query_framework/query_impl.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <base/stable_container.hpp>


namespace compiler::mir {

	base::HashT KeyOf_LowerToMirFunction::customPerfectHash() const {
		return base::perfectHash(function);
	}

	struct InstructionHole;
	struct BlockBuilder;

	using BlockRef = base::StableVectorRef<BlockBuilder>;

	/**
	 * @brief Structure representing block in build process
	 */
	struct BlockBuilder {
	private:
		/**
		 * @brief List of instructions kept in revered order.
		 * If given position does not have a value that means it is empty.
		 */
		std::vector<base::Optional<Instruction>> reversed_instruction;
		base::Optional<Instruction> terminator;
		helios::ScopeID helios_scope;

		struct InstructionHole {
		private:
			BlockRef block_ref;
			usize position;
		public:
			InstructionHole(BlockRef block_ref, usize position):
				block_ref(std::move(block_ref)), position(position) {}
			
			void fill(Instruction instruction) {
				RIFT_ASSERT(block_ref->reversed_instruction.at(position).empty(), "Hole is already filled");
				block_ref->reversed_instruction.at(position).emplace(std::move(instruction));
			}
		};

	public:
		BlockBuilder(helios::ScopeID helios_scope):
			helios_scope(helios_scope) {}
		
		[[nodiscard]]
		Block build() const {
			std::vector<Instruction> instructions;
			for (const auto& instruction: reversed_instruction | std::views::reverse) {
				RIFT_ASSERT(instruction.has_value(), "Empty instruction left in the block");
				instructions.emplace_back(instruction.value());
			}
			return {std::move(instructions), terminator.value(), helios_scope};
		}

		void addInstruction(Instruction instr) {
			reversed_instruction.emplace_back(std::move(instr));
		}

		InstructionHole addHole() {
			reversed_instruction.push_back({});
			
			// creation of borrow pointer here, depends on the fact that blocks
			// are kept in stable container:
			return {base::borrow_ptr(this), reversed_instruction.size() - 1};
		}

		void setTerminator(Instruction instruction) {
			RIFT_ASSERT(not terminator.has_value(), "terminator already set.");
			terminator.emplace(std::move(instruction));
		}
	};

	struct FunctionBuilder {
	private:
		base::StableVector<BlockBuilder> blocks;
		base::Optional<BlockRef> entry_block;
	public:

		Function build() {
			RIFT_ASSERT(entry_block.has_value(), "Entry block not set");
			
			std::vector<Block> blocks;
			for (usize i = 0; i < this->blocks.size(); i++) {
				blocks.emplace_back(this->blocks.getRef(i).value()->build());
			}

			// @TODO: entry block stuff
			
			return Function{blocks};
		}
		
		BlockRef newBlock(helios::ScopeID scope, bool entry = false) {
			auto res = blocks.getRef(blocks.emplaceBack(BlockBuilder{scope})).value();
			if (entry) {
				RIFT_ASSERT(entry_block.empty(), "Entry block already set!");
				entry_block.emplace(res);
			}
			return res;
		}
	};



	// @TODO: HoleID, BlockID, 

	namespace hc = helios::code;

	struct ExprLowerRes {
		BlockRef begin;
		MirLocation value;
	};

	struct StmtLowerRes {
		BlockRef begin;
	};


	struct StmtBlockVisitor: public hc::HoutStmtVisitor {
		BlockRef continuation;

		StmtBlockVisitor(BlockRef continuation):
			continuation(continuation) {}

		base::Optional<StmtLowerRes> out;

		void output(StmtLowerRes value) {
			this->out.emplace(value);
		}

		
		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			throw base::NotYetImplemented("return");
		}
		void visitVoidReturnStmt(const hc::VoidReturnStmt& stmt) override {
			throw base::NotYetImplemented("v return");
		}
		void visitExprStmt(const hc::ExprStmt& stmt) override {
			throw base::NotYetImplemented("expr");
		}
		void visitIfStmt(const hc::IfStmt& stmt) override {
			throw base::NotYetImplemented("if");
		}
	};

	struct ExprBlockVisitor: public helios::code::HoutExprVisitor {
		BlockRef continuation;
	
		base::Optional<ExprLowerRes> out;

		ExprBlockVisitor(BlockRef continuation):
			continuation(continuation) {}

		void output(ExprLowerRes value) {
			this->out.emplace(value);
		}

		void visitConstIntExprMock(const hc::ConstIntExprMock& expr) override {
			throw base::NotYetImplemented("expr");
		}
		void visitIdentifierExpresion(const hc::IdentifierExpresion& expr) override {
			throw base::NotYetImplemented("identifier");
		}
	};

	StmtLowerRes lowerStmt(const hc::Stmt& stmt, BlockRef continuation) {
		StmtBlockVisitor visitor{continuation};
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes lowerExpr(const hc::Expr& expr, BlockRef continuation) {
		ExprBlockVisitor visitor{continuation};
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(const hc::CodeBlock& code_block, BlockRef continuation) {
		StmtLowerRes last_result;
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result = lowerStmt(*stmt, continuation);
			continuation = last_result.begin;
		}
		return last_result;
	}


	// @TODO: StmtExprBoolJmpVisitor for jumping code

	struct IMPLEMENT_QUERY(LowerToMirFunction, Function) {
		static auto provide(Context& ctx, QKey key) -> QResult {
			// First step:
			// * build cfg+quad step by step

			FunctionBuilder function_builder;


			// @TODO:
			// * add some lifetime stuff

			return function_builder.build();
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);



}