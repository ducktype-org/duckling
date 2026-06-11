#include "compiler.hpp"

#include "context.hpp"
#include "function_pass.hpp"
#include "global_pass.hpp"
#include "llvm.hpp"
#include "llvm_lowering_context.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <psac/psac.hpp>

#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

namespace {
	/// One unit of parallel work: a single function definition together with the
	/// namespace it was declared in (so lowering resolves symbols in the right scope).
	struct FunctionWorkItem {
		pst::Access<pst::Fun> stmt;
		pg::ns::NamespaceID   enclosing_ns;
	};

	/**
	 * @brief Walks the PST after GlobalPass and collects every function definition as
	 * a FunctionWorkItem. Mirrors the statement kinds FunctionPass used to traverse.
	 */
	class FunctionCollector final: public pst::PstVisitorPanicky {
	public:
		FunctionCollector(pg::Context& ctx): ctx(ctx) {}

		std::vector<FunctionWorkItem> items;

		void visitFun(pst::Access<pst::Fun> stmt) override {
			items.push_back(FunctionWorkItem{ .stmt = stmt, .enclosing_ns = ctx.currNs().id });
		}

		void visitNamespace(pst::Access<pst::Namespace> stmt) override {
			const base::StrID name = stmt->getName().illegalAccess().value()->unwrap();
			pg::sym::Symbol&  ns_sym
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));
			if (!std::holds_alternative<pg::sym::NamespaceSymbol>(ns_sym.kind))
				throw std::runtime_error("'" + name.str() + "' is not a namespace");

			ctx.enterNamespace(std::get<pg::sym::NamespaceSymbol>(ns_sym.kind).namespace_id);
			for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
				child.illegalAccess().value()->acceptVisitor(*this);
			ctx.popNamespace();
		}

		// Handled entirely by GlobalPass; nothing to lower.
		void visitClass(pst::Access<pst::Class>) override {}

		void visitConst(pst::Access<pst::Const>) override {}

	private:
		pg::Context& ctx;
	};

	/**
	 * @brief Lowers a single function into its own LLVM module and emits an object file.
	 *
	 * Runs on a PSAC worker thread: takes a private copy of the symbol tables (local
	 * declarations mutate them) and a private LLVMContext (LLVM contexts are not
	 * thread-safe). Cross-function calls become declarations resolved by the linker
	 * through the functions' link names.
	 */
	void compileOneFunction(
		const pg::Context&           base_ctx,
		const FunctionWorkItem&      item,
		const std::filesystem::path& object_path
	) {
		pg::Context             ctx = base_ctx;
		pg::LlvmProgramModule   module;
		pg::LlvmLoweringContext lowering(module);
		pg::FunctionPass        pass(ctx, module, lowering);

		if (item.enclosing_ns != ctx.root_ns_id) ctx.enterNamespace(item.enclosing_ns);
		pass.visitFun(item.stmt);

		module.verify();
		module.emitObjectFile(object_path);
	}

	/**
	 * @brief The PSAC computation: one Mod per function, read it and compile that
	 * function. Granularity 1 so every function is an independent parallel task.
	 * Errors are collected per-task (exceptions must not unwind into the scheduler).
	 */
	psac_function(
		compileFunctionsPsac,
		const pg::Context*                        base_ctx,
		const std::vector<FunctionWorkItem>*      items,
		const std::vector<std::filesystem::path>* object_paths,
		std::vector<psac::Mod<u64>>*              inputs,
		std::vector<std::string>*                 errors
	) {
		psac_parallel_for(u64 i, u64{ 0 }, u64{ items->size() }, 1, {
			psac_read((u64 item_index), (&(*inputs)[i]), {
				try {
					compileOneFunction(*base_ctx, (*items)[item_index], (*object_paths)[item_index]);
				} catch (const std::exception& e) { (*errors)[item_index] = e.what(); }
			});
		});
	}

	/**
	 * @brief Links the per-function object files into an executable via the system linker.
	 */
	void makeExecutable(
		const std::vector<std::filesystem::path>& object_paths, std::string_view output_path
	) {
		system_command::SystemCommand linker("clang");
		for (const std::filesystem::path& object_path: object_paths)
			linker.addArg(object_path.string());
		linker.addArg("-o");
		linker.addArg(std::string(output_path));
		if (linker.execute(system_command::SystemCommand::ExitCodeHandling::Warn) != 0)
			CORE_PANIC("Failed to link LLVM objects into executable");

		CORE_DEV_LOG(Compiler, "Wrote executable to ", std::string(output_path), '\n');
	}
}

void pg::compile(const fs::File& file, const CompileOptions& options) {
	CORE_DEV_LOG(
		Compiler,
		"Starting compilation process of ",
		file.getFilePath().string(),
		" to ",
		options.output_path,
		'\n'
	);

	pst::PST pst(file, pst::PSTType::Program);

	Context ctx;

	// Pass 1: register top-level declarations (namespaces, functions, structs, consts).
	GlobalPass global_pass(ctx);
	for (pst::AccessLocked<pst::Stmt> inner_stmt:
	     pst.getRootElement().illegalAccess().value()->getStatements()) {
		inner_stmt.illegalAccess().value()->acceptVisitor(global_pass);
	}

	// Pass 2: collect every function definition as an independent unit of work.
	FunctionCollector collector(ctx);
	for (pst::AccessLocked<pst::Stmt> inner_stmt:
	     pst.getRootElement().illegalAccess().value()->getStatements()) {
		inner_stmt.illegalAccess().value()->acceptVisitor(collector);
	}
	const std::vector<FunctionWorkItem>& items = collector.items;

	std::vector<std::filesystem::path> object_paths;
	object_paths.reserve(items.size());
	for (usize i = 0; i < items.size(); ++i) {
		auto object_path = std::filesystem::path(options.output_path);
		object_path += ".fn" + std::to_string(i) + ".o";
		object_paths.push_back(std::move(object_path));
	}

	// Pass 3: lower function bodies into per-function LLVM modules and object files —
	// sequentially as a baseline, or in parallel through a PSAC computation.
	if (options.sequential) {
		for (usize i = 0; i < items.size(); ++i) compileOneFunction(ctx, items[i], object_paths[i]);
	} else if (!items.empty()) {
		if (options.num_threads > 0) parlay::set_num_workers(options.num_threads);

		std::vector<psac::Mod<u64>> inputs(items.size());
		for (usize i = 0; i < items.size(); ++i) psac_write(&inputs[i], u64(i));
		std::vector<std::string> errors(items.size());

		[[maybe_unused]] auto computation
			= psac_run(compileFunctionsPsac, &ctx, &items, &object_paths, &inputs, &errors);
		psac::GarbageCollector::run();

		for (usize i = 0; i < errors.size(); ++i) {
			if (!errors[i].empty()) {
				throw std::runtime_error(
					"Compilation of function #" + std::to_string(i) + " failed: " + errors[i]
				);
			}
		}
	}

	makeExecutable(object_paths, options.output_path);
}
