#include "compiler.hpp"

#include "llvm.hpp"
#include "psackling/type.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>

#include "base/extend_cpp/strongly_typed_id.hpp"

#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

namespace {
	/**
	 * @brief Turns modules into executables by emitting object files and invoking the system linker.
	 */
	void makeExecutable(pg::LlvmProgramModule& module, std::string_view output_path) {
		const auto executable_path = std::filesystem::path(std::string(output_path));
		auto       object_path     = executable_path;
		object_path += ".o";

		module.emitObjectFile(object_path);

		system_command::SystemCommand linker("clang");
		linker.addArg(object_path.string());
		linker.addArg("-o");
		linker.addArg(executable_path.string());
		if (linker.execute(system_command::SystemCommand::ExitCodeHandling::Warn) != 0)
			CORE_PANIC("Failed to link LLVM object into executable");

		CORE_DEV_LOG(Compiler, "Wrote executable to ", executable_path.string(), '\n');
	}
}

llvm::IntegerType* getIntNTy(llvm::LLVMContext& ctx, std::string_view type_name) {
	if (type_name == "i64") return llvm::Type::getInt64Ty(ctx);
	CORE_PANIC("Unknown type name: " + std::string(type_name));
}

// llvm::Type* getTypeFromExpr(llvm::LLVMContext& ctx, pst::Access<pst::Expr> expr) {
//     if (auto illegal_access = expr.illegalAccess()) {
//         if (auto type_name_access = illegal_access->getTypeName()) {
//             return getIntNTy(ctx, type_name_access->getName().strView());
//         }
//     }
//     CORE_PANIC("Unable to determine type from expression");
// }

class VisitorCompiler final: public pst::PstVisitorPanicky {
public:
	VisitorCompiler(pg::LlvmProgramModule& llvm_module): llvm_module(llvm_module) {}

	pg::LlvmProgramModule& llvm_module;

	void visitReturn(pst::Access<pst::Return> stmt) override {
		llvm_module.inCtxDo([&](llvm::LLVMContext& ctx, llvm::Module&) {
			using namespace llvm;
			class ReturnParser: public pst::expr::PstExprVisitorPanicky {
			public:
				u64  value{};
				void visitExprNumericValue(
					pst::Access<pst::expr::ExprNumericValue> int_literal
				) override {
					value = std::stoull(int_literal->getValue().str());
				}
			};
			ReturnParser parser;
			stmt->getValue()
				->illegalAccess()
				.value()
				->getExpr()
				.illegalAccess()
				.value()
				->acceptExprVisitor(parser);
			builder_stack.back().CreateRet(
				llvm::ConstantInt::get(getIntNTy(ctx, "i64"), parser.value)
			);
		});
	}

	void visitFun(pst::Access<pst::Fun> stmt) override {
		llvm_module.inCtxDo([&](llvm::LLVMContext& ctx, llvm::Module& mod) {
			using namespace llvm;
			IntegerType*  int_type = getIntNTy(ctx, "i64");
			FunctionType* main_ty  = llvm::FunctionType::get(int_type, false);

			Function* main_fn = llvm::Function::Create(
				main_ty, llvm::Function::ExternalLinkage, stmt->getName().strView(), &mod
			);

			pushBlock(ctx, main_fn, "entry");

			pst::Access<pst::CodeBlockOrStmt> body = stmt->getBody().illegalAccess().value();
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     *body->getCodeBlock().illegalAccess().value()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(*this);
			}
			popBlock();
		});
	}

private:
	std::deque<llvm::BasicBlock*> block_stack;
	std::deque<llvm::IRBuilder<>> builder_stack;

	void pushBlock(llvm::LLVMContext& ctx, llvm::Function* func, std::string_view name) {
		auto* entry_block = llvm::BasicBlock::Create(ctx, name, func);
		block_stack.push_back(entry_block);
		builder_stack.emplace_back(entry_block);
	}

	void popBlock() {
		block_stack.pop_back();
		builder_stack.pop_back();
	}
};

void pg::compile(const fs::File& file, std::string_view output_path) {
	CORE_DEV_LOG(
		Compiler,
		"Starting compilation process of ",
		file.getFilePath().string(),
		" to ",
		std::string(output_path),
		'\n'
	);

	pst::PST pst(file, pst::PSTType::Program);

	LlvmProgramModule llvm_module;

	VisitorCompiler compiler(llvm_module);
	for (pst::AccessLocked<pst::Stmt> inner_stmt:
	     pst.getRootElement().illegalAccess().value()->getStatements()) {
		inner_stmt.illegalAccess().value()->acceptVisitor(compiler);
	}

	llvm_module.buildMainReturningZero();
	llvm_module.verify();
	makeExecutable(llvm_module, output_path);
}
