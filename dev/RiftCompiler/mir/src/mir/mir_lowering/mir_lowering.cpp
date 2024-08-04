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

	// @TODO: since BlockRef can be a parameter
	// we need to add BlockBuilderRef->BlockRef transformation
	// during building phase
	using BlockRef = base::StableVectorRef<BlockBuilder>;

	struct ExprLowerRes {
		BlockRef begin;
		MirLocation value;
	};

	struct StmtLowerRes {
		BlockRef begin;
	};

	StmtLowerRes lowerStmt(const hc::Stmt& stmt, BlockRef continuation, FunctionBuilder& function);
	ExprLowerRes lowerExpr(const hc::Expr& expr, BlockRef continuation, FunctionBuilder& function);
	StmtLowerRes lowerCodeBlock(const hc::CodeBlock& code_block, BlockRef continuation, FunctionBuilder& function);

	/**
	 * @brief Structure representing block in build process
	 */
	struct BlockBuilder {
	private:

		BlockID id;

		/**
		 * @brief List of instructions kept in revered order.
		 * If given position does not have a value that means it is empty.
		 */
		std::vector<base::Optional<Instruction>> reversed_instruction;
		base::Optional<Instruction> terminator;

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
		BlockBuilder(usize vector_index): id(vector_index) {};
		
		[[nodiscard]]
		Block build() const {
			std::vector<Instruction> instructions;
			for (const auto& instruction: reversed_instruction | std::views::reverse) {
				RIFT_ASSERT(instruction.has_value(), "Empty instruction left in the block");
				instructions.emplace_back(instruction.value());
			}
			return {id, std::move(instructions), terminator.value()};
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

		[[nodiscard]]
		BlockID getID() const {
			return id;
		}
	};


	struct FunctionBuilder {
	private:
		base::StableVector<BlockBuilder> blocks;
		base::Optional<BlockRef> entry_block;
		base::StableVector<MirLocal> local_list;
	public:

		Function build() {
			RIFT_ASSERT(entry_block.has_value(), "Entry block not set");
			
			std::vector<Block> blocks;
			for (usize i = 0; i < this->blocks.size(); i++) {
				blocks.emplace_back(this->blocks.getRef(i).value()->build());
			}

			// @TODO: entry block stuff
			
			return Function{std::move(blocks), std::move(local_list)};
		}

		LocalRef addLocal() {
			auto key = local_list.emplaceBack(MirLocal{});
			return local_list.getRef(key).value();
		}
		
		BlockRef newBlock(bool entry = false) {
			auto index = blocks.size();
			auto res = blocks.getRef(blocks.emplaceBack(BlockBuilder{index})).value();
			if (entry) {
				RIFT_ASSERT(entry_block.empty(), "Entry block already set!");
				entry_block.emplace(res);
			}
			RIFT_ASSERT(u64(res->getID()) == blocks.size() - 1 , "Bad block id");
			return res;
		}
	};

	struct StmtBlockVisitor: public hc::HoutStmtVisitor {
		BlockRef continuation;

		FunctionBuilder& function;

		StmtBlockVisitor(BlockRef continuation, FunctionBuilder& function):
			continuation(continuation), function(function) {}

		base::Optional<StmtLowerRes> out;

		void output(StmtLowerRes value) {
			this->out.emplace(value);
		}

		
		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			auto return_block = function.newBlock();
	
			// lower expr:
			auto expr_res = lowerExpr(*stmt.value, return_block, function);

			// here return_instruction has to be a terminator: 
			return_block->setTerminator(Instruction(
				Operation::Return,
				{},
				{ expr_res.value },
				{},
				stmt.lifetime_scope
			));

			output({expr_res.begin});			
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

		FunctionBuilder& function;

		ExprBlockVisitor(BlockRef continuation, FunctionBuilder& function):
			continuation(continuation), function(function) {}

		void output(ExprLowerRes value) {
			this->out.emplace(value);
		}

		void visitLiteralValueExpr(const hc::LiteralValueExpr& expr) override {

			throw base::NotYetImplemented("expr");
		}
		void visitIdentifierExpr(const hc::IdentifierExpr& expr) override {
			throw base::NotYetImplemented("identifier");
		}
		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) override {
			throw base::NotYetImplemented("identifier");
		}
	};

	StmtLowerRes lowerStmt(const hc::Stmt& stmt, BlockRef continuation, FunctionBuilder& function) {
		StmtBlockVisitor visitor{continuation, function};
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes lowerExpr(const hc::Expr& expr, BlockRef continuation, FunctionBuilder& function) {
		ExprBlockVisitor visitor{continuation, function};
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(const hc::CodeBlock& code_block, BlockRef continuation, FunctionBuilder& function) {
		StmtLowerRes last_result { continuation };
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result = lowerStmt(*stmt, continuation, function);
			continuation = last_result.begin;
		}
		return last_result;
	}


	// @TODO: StmtExprBoolJmpVisitor for jumping code

	struct IMPLEMENT_QUERY(LowerToMirFunction, Function) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// First step:
			// * build cfg+quad step by step


			FunctionBuilder function_builder;

			// @TODO: add parameters stuff

			auto fun_body_scope = key.function.body.body->lifetime_scope;

			auto last_block = function_builder.newBlock();
			last_block->setTerminator({Operation::FunctionEnd, {}, {}, {}, fun_body_scope});

			auto first_block = lowerCodeBlock(*key.function.body.body, last_block, function_builder);

			auto entry_block = function_builder.newBlock(true);

			// @TODO: jump arguments:
			entry_block->setTerminator({ Operation::Jump, {}, {first_block.begin->getID()}, {}, fun_body_scope} );


			// @TODO:
			// * add some lifetime stuff

			return function_builder.build();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);



}
