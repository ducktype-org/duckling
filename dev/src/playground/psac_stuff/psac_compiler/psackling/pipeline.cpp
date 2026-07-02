#include "pipeline.hpp"

#include "expr_lowering.hpp"
#include "function_pass.hpp"
#include "global_pass.hpp"
#include "llvm.hpp"
#include "llvm_lowering_context.hpp"

LLVM_INCLUDE_BEGIN()
#include <llvm/Transforms/Utils/ModuleUtils.h>
LLVM_INCLUDE_END()

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>

#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

#include <cstdlib>
#include <sstream>

namespace {
	/// Content hash of a PST element: hash of its recursive debug print. The dprint
	/// output covers names and values of the whole subtree but no source positions, so
	/// it is stable under reformatting/moving and across files — exactly what the
	/// incremental diff needs. Template because dprint is public only on the concrete
	/// element types.
	template<typename Element>
	u64 contentHash(const Element& element) {
		std::ostringstream out;
		element.dprint(out);
		return std::hash<std::string>{}(std::move(out).str());
	}

	u64 combineHash(u64 seed, u64 value) {
		// FNV-style mix; quality is plenty for change detection.
		return (seed ^ value) * 1'099'511'628'211ULL;
	}

	/**
	 * @brief Walks the PST after GlobalPass and collects every function definition as
	 * a FunctionWorkItem (with its content hash and namespace-qualified link name).
	 */
	class FunctionCollector final: public pst::PstVisitorPanicky {
	public:
		FunctionCollector(pg::Context& ctx): ctx(ctx) {}

		std::vector<pg::FunctionWorkItem> items;
		std::vector<u64>                  fn_hashes;
		std::vector<std::string>          link_names;
		std::vector<pg::GlobalInitItem>   global_inits;
		std::vector<pg::ConstInputInfo>   const_inputs;

		void visitFun(pst::Access<pst::Fun> stmt) override {
			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& fun_sym
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));

			items.push_back(pg::FunctionWorkItem{ .stmt = stmt, .enclosing_ns = ctx.currNs().id });
			fn_hashes.push_back(contentHash(*stmt));
			link_names.push_back(std::get<pg::sym::FunctionSymbol>(fun_sym.kind).link_name.str());
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

		// Handled by GlobalPass; nothing to lower per-function.
		void visitImport(pst::Access<pst::Import>) override {}

		// Handled entirely by GlobalPass; nothing to lower per-function.
		void visitClass(pst::Access<pst::Class>) override {}

		void visitConst(pst::Access<pst::Const> stmt) override {
			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& symbol
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));
			const_inputs.push_back(pg::ConstInputInfo{ .name_hash = pg::constNameHash(symbol),
			                                           .value_hash
			                                           = pg::constValueHash(ctx, symbol) });
		}

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			if (stmt->getValue().has_value())
				global_inits.push_back(pg::GlobalInitItem{ .stmt         = stmt,
				                                           .enclosing_ns = ctx.currNs().id });
		}

	private:
		pg::Context& ctx;
	};

	/**
	 * @brief Hashes everything a function may depend on besides its own body: classes,
	 * constants, namespace structure and the signatures (not bodies) of all functions.
	 * If this hash changes, every function is recompiled — the coarse half of the
	 * dependency tracking, where the real compiler tracks fine-grained query inputs.
	 */
	class GlobalsHasher final: public pst::PstVisitorPanicky {
	public:
		u64 hash = 14'695'981'039'346'656'037ULL;

		/// Constants are accumulated separately: the PSAC backend folds them into the
		/// coarse globals hash, while the query backend tracks them per constant.
		u64 consts_hash = 14'695'981'039'346'656'037ULL;

		void visitClass(pst::Access<pst::Class> stmt) override {
			hash = combineHash(hash, contentHash(*stmt));
		}

		void visitConst(pst::Access<pst::Const> stmt) override {
			consts_hash = combineHash(consts_hash, contentHash(*stmt));
		}

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			hash = combineHash(hash, contentHash(*stmt));
		}

		void visitFun(pst::Access<pst::Fun> stmt) override {
			// Signature only: name, parameters, return type. The body is covered by the
			// function's own Mod.
			hash = combineHash(
				hash,
				std::hash<std::string>{}(stmt->getName().illegalAccess().value()->unwrap().str())
			);
			hash = combineHash(hash, contentHash(*stmt->getParams().illegalAccess().value()));
			if (stmt->getRet().has_value())
				hash = combineHash(hash, contentHash(*stmt->getRet()->illegalAccess().value()));
		}

		void visitNamespace(pst::Access<pst::Namespace> stmt) override {
			hash = combineHash(
				hash,
				std::hash<std::string>{}(stmt->getName().illegalAccess().value()->unwrap().str())
			);
			for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
				child.illegalAccess().value()->acceptVisitor(*this);
		}

		// Rewiring an import can change what every use-site resolves to.
		void visitImport(pst::Access<pst::Import> stmt) override {
			hash = combineHash(hash, contentHash(*stmt));
		}
	};

	/**
	 * @brief Defines the builtin runtime functions in the globals module. Matches the
	 * real compiler's builtins:
	 *   int64_t builtin_output_i64(int64_t v)  — printf("%ld\n", v)
	 *   int64_t builtin_output_char(char c)    — printf("%c", c)
	 * Both return printf's result.
	 */
	void defineBuiltins(pg::LlvmProgramModule& module) {
		module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module& mod) {
			auto* i64_ty = llvm::Type::getInt64Ty(llvm_ctx);
			auto* i32_ty = llvm::Type::getInt32Ty(llvm_ctx);
			auto* i8_ty  = llvm::Type::getInt8Ty(llvm_ctx);
			auto* ptr_ty = llvm::PointerType::getUnqual(llvm_ctx);

			auto* printf_ty = llvm::FunctionType::get(i32_ty, { ptr_ty }, /*isVarArg=*/true);
			auto* printf_fn
				= llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf", &mod);

			{
				auto* output_ty = llvm::FunctionType::get(i64_ty, { i64_ty }, false);
				auto* output_fn = llvm::Function::Create(
					output_ty, llvm::Function::ExternalLinkage, "builtin_output_i64", &mod
				);

				auto*             entry = llvm::BasicBlock::Create(llvm_ctx, "entry", output_fn);
				llvm::IRBuilder<> builder(entry);
				llvm::Value*      format = builder.CreateGlobalStringPtr("%ld\n", "output_i64_fmt");
				llvm::Value*      printed
					= builder.CreateCall(printf_ty, printf_fn, { format, output_fn->getArg(0) });
				builder.CreateRet(builder.CreateSExt(printed, i64_ty));
			}

			{
				auto* output_ty = llvm::FunctionType::get(i64_ty, { i8_ty }, false);
				auto* output_fn = llvm::Function::Create(
					output_ty, llvm::Function::ExternalLinkage, "builtin_output_char", &mod
				);

				auto*             entry = llvm::BasicBlock::Create(llvm_ctx, "entry", output_fn);
				llvm::IRBuilder<> builder(entry);
				llvm::Value*      format = builder.CreateGlobalStringPtr("%c", "output_char_fmt");
				// C default argument promotion: char widens to int before the vararg call.
				llvm::Value* widened = builder.CreateSExt(output_fn->getArg(0), i32_ty);
				llvm::Value* printed
					= builder.CreateCall(printf_ty, printf_fn, { format, widened });
				builder.CreateRet(builder.CreateSExt(printed, i64_ty));
			}
		});
	}
}

namespace pg {
	AnalyzedProgram analyzeProgram(const fs::File& file) {
		AnalyzedProgram program;
		{
			PhaseTimer timer("parse");
			program.modules = loadModules(file);
		}

		auto for_each_top_level = [](const LoadedModule& module, auto& visitor) {
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     module.pst->getRootElement().illegalAccess().value()->getStatements()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(visitor);
			}
		};

		// Runs the visitor over every module's top level inside the module's namespace
		// chain. Modules come dependency-first, so imports always resolve.
		auto for_each_module = [&](auto& visitor) {
			for (const LoadedModule& module: program.modules) {
				const usize entered = program.ctx.enterModulePath(module.module_path);
				for_each_top_level(module, visitor);
				for (usize i = 0; i < entered; ++i) program.ctx.popNamespace();
			}
		};

		// Pass 1: register top-level declarations (namespaces, functions, structs, consts).
		{
			PhaseTimer timer("global_pass");
			GlobalPass global_pass(program.ctx);
			for_each_module(global_pass);
		}

		// Pass 2: collect function work items, content hashes and the globals hash.
		{
			PhaseTimer        timer("collect+hash");
			FunctionCollector collector(program.ctx);
			for_each_module(collector);
			program.items        = std::move(collector.items);
			program.fn_hashes    = std::move(collector.fn_hashes);
			program.link_names   = std::move(collector.link_names);
			program.global_inits = std::move(collector.global_inits);
			program.const_inputs = std::move(collector.const_inputs);

			GlobalsHasher globals_hasher;
			for (const LoadedModule& module: program.modules) {
				// Module identity participates: moving a definition between files
				// changes link names even when content hashes stay equal.
				for (base::StrID component: module.module_path)
					globals_hasher.hash = combineHash(
						globals_hasher.hash, std::hash<std::string>{}(component.str())
					);
				for_each_top_level(module, globals_hasher);
			}
			program.globals_hash_no_consts = globals_hasher.hash;
			program.globals_hash = combineHash(globals_hasher.hash, globals_hasher.consts_hash);
		}

		return program;
	}

	u64 constNameHash(const sym::Symbol& symbol) {
		return std::hash<std::string>{}(
			std::get<sym::ConstantSymbol>(symbol.kind).qualified_name.str()
		);
	}

	u64 constValueHash(Context& ctx, const sym::Symbol& symbol) {
		const u64 type_hash = std::hash<std::string>{}(ctx.getTypeInfo(symbol.type_id).name.str());
		return combineHash(std::get<sym::ConstantSymbol>(symbol.kind).bits, type_hash);
	}

	void compileOneFunction(
		const Context&               base_ctx,
		const FunctionWorkItem&      item,
		const std::filesystem::path& object_path,
		SymbolUseObserver*           observer
	) {
		Context ctx             = base_ctx;
		ctx.symbol_use_observer = observer;
		LlvmProgramModule   module;
		LlvmLoweringContext lowering(module);
		FunctionPass        pass(ctx, module, lowering);

		if (item.enclosing_ns != ctx.root_ns_id) ctx.enterNamespace(item.enclosing_ns);
		pass.visitFun(item.stmt);

		module.verify();
		module.emitObjectFile(object_path);
	}

	void emitGlobalsObject(
		const Context&                     base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
		const std::filesystem::path&       object_path,
		SymbolUseObserver*                 observer
	) {
		Context ctx             = base_ctx;
		ctx.symbol_use_observer = observer;
		LlvmProgramModule   module;
		LlvmLoweringContext lowering(module);

		// Created through the lowering cache so the initializer expressions below reuse
		// the same llvm::GlobalVariable instead of creating clashing declarations.
		for (sym::SymbolID global_sym_id: ctx.global_variables) {
			sym::Symbol&          global_sym = ctx.getSymbol(global_sym_id);
			llvm::GlobalVariable* global     = lowering.getOrCreateGlobal(global_sym_id, ctx);
			llvm::Type*           llvm_ty    = lowering.lowerType(global_sym.type_id, ctx);
			global->setInitializer(llvm::Constant::getNullValue(llvm_ty));
		}

		defineBuiltins(module);

		if (!global_inits.empty()) {
			module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module& mod) {
				auto* init_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(llvm_ctx), false);
				auto* init_fn = llvm::Function::Create(
					init_ty, llvm::Function::InternalLinkage, "psack_global_init", &mod
				);
				auto*             entry = llvm::BasicBlock::Create(llvm_ctx, "entry", init_fn);
				llvm::IRBuilder<> builder(entry);
				LocalsMap         no_locals;

				for (const GlobalInitItem& item: global_inits) {
					const base::StrID name = item.stmt->getName().illegalAccess().value()->unwrap();
					sym::SymbolID global_sym_id = ctx.lookupInNamespace(item.enclosing_ns, name);
					sym::Symbol&  global_sym    = ctx.getSymbol(global_sym_id);

					const pst::ExprElement& init_expr = *item.stmt->getValue()
					                                         ->illegalAccess()
					                                         .value()
					                                         ->getExpr()
					                                         .illegalAccess()
					                                         .value();

					if (item.enclosing_ns != ctx.root_ns_id) ctx.enterNamespace(item.enclosing_ns);
					ExprLowering expr_lowering(ctx, lowering, builder, no_locals);
					LoweredExpr  init = expr_lowering.lower(init_expr);
					if (item.enclosing_ns != ctx.root_ns_id) ctx.popNamespace();

					if (init.type_id != global_sym.type_id) {
						throw std::runtime_error(
							"Global variable '" + name.str()
							+ "': initializer type does not match declared type"
						);
					}
					builder.CreateStore(init.value, lowering.getOrCreateGlobal(global_sym_id, ctx));
				}
				builder.CreateRetVoid();

				llvm::appendToGlobalCtors(mod, init_fn, /*Priority=*/65'535);
			});
		}

		module.verify();
		module.emitObjectFile(object_path);
	}

	void linkObjects(
		const std::vector<std::filesystem::path>& object_paths,
		const std::filesystem::path&              globals_object_path,
		const std::string&                        output_path
	) {
		system_command::SystemCommand linker("clang");
		for (const std::filesystem::path& object_path: object_paths)
			linker.addArg(object_path.string());
		linker.addArg(globals_object_path.string());
		linker.addArg("-o");
		linker.addArg(output_path);
		if (linker.execute(system_command::SystemCommand::ExitCodeHandling::Warn) != 0)
			CORE_PANIC("Failed to link LLVM objects into executable");

		CORE_DEV_LOG(Compiler, "Wrote executable to ", output_path, '\n');
	}

	PhaseTimer::PhaseTimer(const char* phase): phase(phase) {
		if (enabled()) start = std::chrono::steady_clock::now();
	}

	PhaseTimer::~PhaseTimer() {
		if (!enabled()) return;
		const auto elapsed = std::chrono::steady_clock::now() - start;
		std::cerr << "[timing] " << phase << ": "
				  << std::chrono::duration<double, std::milli>(elapsed).count() << " ms\n";
	}

	bool PhaseTimer::enabled() {
		// NOLINTNEXTLINE(concurrency-mt-unsafe) — read once before any threads race.
		static const bool on = std::getenv("PSACKLINGER_TIMING") != nullptr;
		return on;
	}
}
