#include "compiler.hpp"

#include "context.hpp"
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
#include <psac/psac.hpp>

#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <thread>

namespace {
	/// parlay's scheduler is created during static initialization (a global
	/// block_allocator calls parallel_for) and its idle workers busy-spin, roughly
	/// doubling the serial front-end time through core/cache contention. Keep the pool
	/// parked at a single worker except around the parallel lowering phase.
	void parkScheduler() { parlay::set_num_workers(1); }

	void unparkScheduler(usize num_threads) {
		parlay::set_num_workers(
			num_threads > 0 ? num_threads : usize(std::thread::hardware_concurrency())
		);
	}

	/// Wall-clock phase timer, enabled with PSACKLINGER_TIMING=1; prints to stderr.
	class PhaseTimer final {
	public:
		PhaseTimer(const char* phase): phase(phase) {
			if (enabled()) start = std::chrono::steady_clock::now();
		}

		~PhaseTimer() {
			if (!enabled()) return;
			const auto elapsed = std::chrono::steady_clock::now() - start;
			std::cerr << "[timing] " << phase << ": "
					  << std::chrono::duration<double, std::milli>(elapsed).count() << " ms\n";
		}

		static bool enabled() {
			// NOLINTNEXTLINE(concurrency-mt-unsafe) — read once before any threads race.
			static const bool on = std::getenv("PSACKLINGER_TIMING") != nullptr;
			return on;
		}

	private:
		const char*                           phase;
		std::chrono::steady_clock::time_point start;
	};

	/// One unit of parallel work: a single function definition together with the
	/// namespace it was declared in (so lowering resolves symbols in the right scope).
	struct FunctionWorkItem {
		pst::Access<pst::Fun> stmt;
		pg::ns::NamespaceID   enclosing_ns;
	};

	/// A global variable with an initializer expression. Lowered (in declaration
	/// order) into the synthetic constructor of the globals object file.
	struct GlobalInitItem {
		pst::Access<pst::Variable> stmt;
		pg::ns::NamespaceID        enclosing_ns;
	};

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

		std::vector<FunctionWorkItem> items;
		std::vector<u64>              fn_hashes;
		std::vector<std::string>      link_names;
		std::vector<GlobalInitItem>   global_inits;

		void visitFun(pst::Access<pst::Fun> stmt) override {
			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& fun_sym
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));

			items.push_back(FunctionWorkItem{ .stmt = stmt, .enclosing_ns = ctx.currNs().id });
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

		// Handled entirely by GlobalPass; nothing to lower per-function.
		void visitClass(pst::Access<pst::Class>) override {}

		void visitConst(pst::Access<pst::Const>) override {}

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			if (stmt->getValue().has_value())
				global_inits.push_back(GlobalInitItem{ .stmt         = stmt,
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

		void visitClass(pst::Access<pst::Class> stmt) override {
			hash = combineHash(hash, contentHash(*stmt));
		}

		void visitConst(pst::Access<pst::Const> stmt) override {
			hash = combineHash(hash, contentHash(*stmt));
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
	 * @brief Defines the builtin runtime functions in the globals module. Matches the
	 * real compiler's builtin: int64_t builtin_output_i64(int64_t v) — prints the value
	 * and a newline via libc printf and returns printf's result.
	 */
	void defineBuiltins(pg::LlvmProgramModule& module) {
		module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module& mod) {
			auto* i64_ty = llvm::Type::getInt64Ty(llvm_ctx);
			auto* i32_ty = llvm::Type::getInt32Ty(llvm_ctx);
			auto* ptr_ty = llvm::PointerType::getUnqual(llvm_ctx);

			auto* printf_ty = llvm::FunctionType::get(i32_ty, { ptr_ty }, /*isVarArg=*/true);
			auto* printf_fn
				= llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf", &mod);

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
		});
	}

	/**
	 * @brief Emits the object file that defines all global variables (zero-initialized)
	 * and the builtin runtime functions. Initializer expressions — which may call user
	 * functions and read other globals — are lowered in declaration order into a
	 * synthetic constructor registered in llvm.global_ctors, so libc runs it before
	 * main. Function modules only declare the globals; this is the single definition
	 * the linker resolves them against.
	 */
	void emitGlobalsObject(
		const pg::Context&                 base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
		const std::filesystem::path&       object_path
	) {
		pg::Context             ctx = base_ctx;
		pg::LlvmProgramModule   module;
		pg::LlvmLoweringContext lowering(module);

		// Created through the lowering cache so the initializer expressions below reuse
		// the same llvm::GlobalVariable instead of creating clashing declarations.
		for (pg::sym::SymbolID global_sym_id: ctx.global_variables) {
			pg::sym::Symbol&      global_sym = ctx.getSymbol(global_sym_id);
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
				pg::LocalsMap     no_locals;

				for (const GlobalInitItem& item: global_inits) {
					const base::StrID name = item.stmt->getName().illegalAccess().value()->unwrap();
					pg::sym::SymbolID global_sym_id
						= ctx.lookupInNamespace(item.enclosing_ns, name);
					pg::sym::Symbol& global_sym = ctx.getSymbol(global_sym_id);

					const pst::ExprElement& init_expr = *item.stmt->getValue()
					                                         ->illegalAccess()
					                                         .value()
					                                         ->getExpr()
					                                         .illegalAccess()
					                                         .value();

					if (item.enclosing_ns != ctx.root_ns_id) ctx.enterNamespace(item.enclosing_ns);
					pg::ExprLowering expr_lowering(ctx, lowering, builder, no_locals);
					pg::LoweredExpr  init = expr_lowering.lower(init_expr);
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

	/**
	 * @brief The PSAC computation: every function's compilation is a read of its
	 * content-hash Mod plus the shared globals-hash Mod, so psac_propagate re-runs it
	 * exactly when one of those was rewritten. Granularity 1 makes each function an
	 * independent parallel task. Errors are collected per-task (exceptions must not
	 * unwind into the scheduler).
	 */
	psac_function(
		compileFunctionsPsac,
		const pg::Context*                        base_ctx,
		const std::vector<FunctionWorkItem>*      items,
		const std::vector<std::filesystem::path>* object_paths,
		std::vector<psac::Mod<u64>>*              fn_mods,
		psac::Mod<u64>*                           globals_mod,
		std::vector<std::string>*                 errors,
		std::atomic<usize>*                       recompile_count
	) {
		psac_parallel_for(u64 i, u64{ 0 }, u64{ items->size() }, 1, {
			psac_read((u64 fn_hash, u64 globals_hash), (&(*fn_mods)[i], globals_mod), {
				(void) fn_hash;
				(void) globals_hash;
				recompile_count->fetch_add(1, std::memory_order_relaxed);
				try {
					compileOneFunction(*base_ctx, (*items)[i], (*object_paths)[i]);
				} catch (const std::exception& e) { (*errors)[i] = e.what(); }
			});
		});
	}
}

namespace pg {
	struct Compiler::AnalyzedProgram {
		std::unique_ptr<pst::PST<>>   pst;
		Context                       ctx;
		std::vector<FunctionWorkItem> items;
		std::vector<u64>              fn_hashes;
		std::vector<std::string>      link_names;
		std::vector<GlobalInitItem>   global_inits;
		u64                           globals_hash = 0;
	};

	struct Compiler::State {
		CompileOptions options;

		// The PST must outlive the work items (they hold Access references into it).
		std::unique_ptr<pst::PST<>>   pst;
		Context                       ctx;
		std::vector<FunctionWorkItem> items;
		std::vector<u64>              fn_hashes;
		std::vector<std::string>      link_names;
		std::vector<GlobalInitItem>   global_inits;
		u64                           globals_hash = 0;

		std::vector<std::filesystem::path> object_paths;
		std::filesystem::path              globals_object_path;

		// PSAC inputs/outputs. The read closures capture raw pointers to all of these,
		// so they live here (stable addresses) and are only replaced after the
		// computation that references them is destroyed.
		std::vector<psac::Mod<u64>>      fn_mods;
		psac::Mod<u64>                   globals_mod;
		std::vector<std::string>         errors;
		std::atomic<usize>               recompile_count{ 0 };
		std::optional<psac::Computation> computation;
	};

	Compiler::Compiler(CompileOptions options): state(std::make_unique<State>()) {
		state->options = std::move(options);
		parkScheduler();
	}

	Compiler::~Compiler() = default;

	Compiler::AnalyzedProgram Compiler::analyze(const fs::File& file) {
		AnalyzedProgram program;
		{
			PhaseTimer timer("parse");
			// Skip the PST's SHA-256 stable hashes — incremental diffing here uses its
			// own dprint-based content hashes instead.
			program.pst = std::make_unique<pst::PST<>>(
				file, pst::PSTType::Program, hashing::ComponentHash{}, pst::PSTHashing::Skip
			);
		}

		auto for_each_top_level = [&](auto& visitor) {
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     program.pst->getRootElement().illegalAccess().value()->getStatements()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(visitor);
			}
		};

		// Pass 1: register top-level declarations (namespaces, functions, structs, consts).
		{
			PhaseTimer timer("global_pass");
			GlobalPass global_pass(program.ctx);
			for_each_top_level(global_pass);
		}

		// Pass 2: collect function work items, content hashes and the globals hash.
		{
			PhaseTimer        timer("collect+hash");
			FunctionCollector collector(program.ctx);
			for_each_top_level(collector);
			program.items        = std::move(collector.items);
			program.fn_hashes    = std::move(collector.fn_hashes);
			program.link_names   = std::move(collector.link_names);
			program.global_inits = std::move(collector.global_inits);

			GlobalsHasher globals_hasher;
			for_each_top_level(globals_hasher);
			program.globals_hash = globals_hasher.hash;
		}

		return program;
	}

	void Compiler::throwOnErrors() const {
		for (usize i = 0; i < state->errors.size(); ++i) {
			if (!state->errors[i].empty()) {
				throw std::runtime_error(
					"Compilation of function '" + state->link_names[i]
					+ "' failed: " + state->errors[i]
				);
			}
		}
	}

	void Compiler::link() const {
		system_command::SystemCommand linker("clang");
		for (const std::filesystem::path& object_path: state->object_paths)
			linker.addArg(object_path.string());
		linker.addArg(state->globals_object_path.string());
		linker.addArg("-o");
		linker.addArg(state->options.output_path);
		if (linker.execute(system_command::SystemCommand::ExitCodeHandling::Warn) != 0)
			CORE_PANIC("Failed to link LLVM objects into executable");

		CORE_DEV_LOG(Compiler, "Wrote executable to ", state->options.output_path, '\n');
	}

	CompileStats Compiler::compileAnalyzed(AnalyzedProgram program) {
		State& s = *state;

		// The previous computation holds pointers into the Mod vector; drop it before
		// replacing anything it references.
		s.computation.reset();
		psac::GarbageCollector::run();

		const usize n = program.items.size();

		s.pst          = std::move(program.pst);
		s.ctx          = program.ctx;
		s.items        = std::move(program.items);
		s.fn_hashes    = std::move(program.fn_hashes);
		s.link_names   = std::move(program.link_names);
		s.global_inits = std::move(program.global_inits);
		s.globals_hash = program.globals_hash;

		s.object_paths.clear();
		for (usize i = 0; i < n; ++i) {
			auto object_path = std::filesystem::path(s.options.output_path);
			object_path += ".fn" + std::to_string(i) + ".o";
			s.object_paths.push_back(std::move(object_path));
		}

		s.globals_object_path = std::filesystem::path(s.options.output_path);
		s.globals_object_path += ".globals.o";
		{
			PhaseTimer timer("globals_object");
			emitGlobalsObject(s.ctx, s.global_inits, s.globals_object_path);
		}

		s.fn_mods = std::vector<psac::Mod<u64>>(n);
		for (usize i = 0; i < n; ++i) psac_write(&s.fn_mods[i], s.fn_hashes[i]);
		psac_write(&s.globals_mod, s.globals_hash);

		s.errors.assign(n, "");
		s.recompile_count.store(0);

		{
			PhaseTimer timer("function_lowering");
			if (s.options.sequential) {
				for (usize i = 0; i < n; ++i)
					compileOneFunction(s.ctx, s.items[i], s.object_paths[i]);
			} else if (n > 0) {
				unparkScheduler(s.options.num_threads);
				// psac_run expands to a statement (trailing semicolon), so it can't be nested
				// inside a call expression.
				auto computation = psac_run(
									   compileFunctionsPsac,
									   &s.ctx,
									   &s.items,
									   &s.object_paths,
									   &s.fn_mods,
									   &s.globals_mod,
									   &s.errors,
									   &s.recompile_count
				) s.computation.emplace(std::move(computation));
				parkScheduler();
			}
		}

		throwOnErrors();
		{
			PhaseTimer timer("link");
			link();
		}
		return CompileStats{ .total_functions = n, .recompiled_functions = n };
	}

	CompileStats Compiler::compile(const fs::File& file) { return compileAnalyzed(analyze(file)); }

	CompileStats Compiler::update(const fs::File& file) {
		State& s = *state;

		AnalyzedProgram program = analyze(file);

		// No previous compilation, or the set of functions changed (added/removed/
		// renamed): the computation shape no longer matches, do a full build.
		const bool same_function_set = (s.pst != nullptr) && (program.link_names == s.link_names);
		if (!same_function_set) return compileAnalyzed(std::move(program));

		const usize n = s.items.size();

		// Refresh shared state in place — the PSAC read closures point at these. The
		// old PST is destroyed only after the items referencing it were replaced.
		s.ctx          = program.ctx;
		s.items        = std::move(program.items);
		s.global_inits = std::move(program.global_inits);
		s.pst          = std::move(program.pst);

		// Diff hashes and rewrite exactly the Mods that changed; propagation then
		// re-runs exactly the reads of those Mods.
		std::vector<usize> changed;
		for (usize i = 0; i < n; ++i) {
			if (program.fn_hashes[i] != s.fn_hashes[i]) {
				changed.push_back(i);
				if (!s.options.sequential) psac_write(&s.fn_mods[i], program.fn_hashes[i]);
			}
		}
		const bool globals_changed = program.globals_hash != s.globals_hash;
		if (globals_changed) {
			emitGlobalsObject(s.ctx, s.global_inits, s.globals_object_path);
			if (!s.options.sequential) psac_write(&s.globals_mod, program.globals_hash);
		}

		s.fn_hashes    = std::move(program.fn_hashes);
		s.globals_hash = program.globals_hash;

		if (changed.empty() && !globals_changed)
			return CompileStats{ .total_functions = n, .recompiled_functions = 0 };

		s.errors.assign(n, "");
		s.recompile_count.store(0);

		if (s.options.sequential) {
			if (globals_changed) {
				for (usize i = 0; i < n; ++i)
					compileOneFunction(s.ctx, s.items[i], s.object_paths[i]);
				s.recompile_count.store(n);
			} else {
				for (usize i: changed) compileOneFunction(s.ctx, s.items[i], s.object_paths[i]);
				s.recompile_count.store(changed.size());
			}
		} else {
			unparkScheduler(s.options.num_threads);
			psac_propagate(s.computation.value());
			psac::GarbageCollector::run();
			parkScheduler();
		}

		throwOnErrors();
		link();
		return CompileStats{ .total_functions      = n,
			                 .recompiled_functions = s.recompile_count.load() };
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

	Compiler compiler(options);
	compiler.compile(file);
}
