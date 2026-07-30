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

#include <hashing/hash.hpp>
#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
	/// dprint prefixes every element with its source position ("position":
	/// {"sourceStart": N, "sourceEnd": M}); strip those before hashing, or any byte
	/// shift (added line, extra space) would change the hash of every element after
	/// it and defeat the incremental diff.
	std::string stripPositions(const std::string& printed) {
		static constexpr std::string_view MARKER = "\"position\": {";

		std::string out;
		out.reserve(printed.size());
		usize pos = 0;
		while (true) {
			const usize found = printed.find(MARKER, pos);
			if (found == std::string::npos) {
				out.append(printed, pos, std::string::npos);
				return out;
			}
			out.append(printed, pos, found - pos);
			// The position object is flat (no nested braces).
			usize end = printed.find('}', found);
			CORE_ASSERT(end != std::string::npos, "Unterminated position object in dprint output");
			end += 1;
			if (printed.compare(end, 2, ", ") == 0) end += 2;
			pos = end;
		}
	}

	/// Content hash of a PST element: SHA-256 of its recursive debug print with
	/// source positions stripped, so it is stable under reformatting/moving and
	/// across files — exactly what the incremental diff needs. Template because
	/// dprint is public only on the concrete element types.
	template<typename Element>
	base::Bit256 contentHash(const Element& element) {
		std::ostringstream out;
		element.dprint(out);
		return hashing::justHash<hashing::SHA256>(stripPositions(std::move(out).str()));
	}

	/// Hash a variable-length string field with a length prefix, so adjacent
	/// fields can never alias across their boundary ("ab","c" vs "a","bc").
	void addField(hashing::StatefulHash<hashing::SHA256>& hash, std::string_view field) {
		hash(field.size());
		hash(field);
	}

	void writeFileBytes(const std::filesystem::path& path, const std::string& bytes) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) CORE_PANIC("Unable to create file: " + path.string());
		out.write(bytes.data(), std::streamsize(bytes.size()));
		if (!out) CORE_PANIC("Failed to write file: " + path.string());
	}

	/**
	 * @brief Walks the PST after GlobalPass and collects every function definition as
	 * a FunctionWorkItem (with its content hash and namespace-qualified link name).
	 */
	class FunctionCollector final: public pst::PstVisitorPanicky {
	public:
		FunctionCollector(pg::Context& ctx): ctx(ctx) {}

		std::vector<pg::FunctionWorkItem> items;
		std::vector<base::Bit256>         fn_hashes;
		std::vector<std::string>          link_names;
		std::vector<pg::GlobalInitItem>   global_inits;
		std::vector<pg::SymbolInputInfo>  const_inputs;
		std::vector<pg::SymbolInputInfo>  sig_inputs;
		std::vector<pg::SymbolInputInfo>  class_inputs;
		std::vector<pg::SymbolInputInfo>  global_var_inputs;

		void visitFun(pst::Access<pst::Fun> stmt) override {
			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& fun_sym
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));

			items.push_back(pg::FunctionWorkItem{ .stmt = stmt, .enclosing_ns = ctx.currNs().id });
			fn_hashes.push_back(contentHash(*stmt));
			link_names.push_back(std::get<pg::sym::FunctionSymbol>(fun_sym.kind).link_name.str());
			sig_inputs.push_back(pg::SymbolInputInfo{ .key = pg::functionSigInputKey(fun_sym),
			                                          .input_hash
			                                          = pg::functionSigInputHash(ctx, fun_sym) });
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

		void visitClass(pst::Access<pst::Class> stmt) override {
			// Declared by GlobalPass; only the layout input is collected here.
			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& symbol
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));
			const pg::tp::TypeInfo& type_info = ctx.getTypeInfo(symbol.type_id);
			class_inputs.push_back(pg::SymbolInputInfo{
				.key        = pg::classLayoutInputKey(type_info),
				.input_hash = pg::classLayoutInputHash(ctx, type_info) });
		}

		void visitConst(pst::Access<pst::Const> stmt) override {
			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& symbol
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));
			const_inputs.push_back(pg::SymbolInputInfo{
				.key = pg::constInputKey(symbol), .input_hash = pg::constInputHash(ctx, symbol) });
		}

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			if (stmt->getValue().has_value())
				global_inits.push_back(pg::GlobalInitItem{ .stmt         = stmt,
				                                           .enclosing_ns = ctx.currNs().id });

			const base::StrID      name = stmt->getName().illegalAccess().value()->unwrap();
			const pg::sym::Symbol& symbol
				= ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));
			global_var_inputs.push_back(pg::SymbolInputInfo{
				.key        = pg::globalVarInputKey(symbol),
				.input_hash = pg::globalVarInputHash(ctx, symbol) });
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
		/// Structural part: imports, namespace names and module identity. The query
		/// backend keeps only this coarse; everything below is accumulated separately
		/// (folded back into the full hash for the PSAC backend, tracked per symbol
		/// by the query backend).
		hashing::StatefulHash<hashing::SHA256> structural;

		hashing::StatefulHash<hashing::SHA256> consts;
		hashing::StatefulHash<hashing::SHA256> sigs;
		hashing::StatefulHash<hashing::SHA256> classes;
		hashing::StatefulHash<hashing::SHA256> vars;

		void visitClass(pst::Access<pst::Class> stmt) override { classes(contentHash(*stmt)); }

		void visitConst(pst::Access<pst::Const> stmt) override { consts(contentHash(*stmt)); }

		void visitVariable(pst::Access<pst::Variable> stmt) override { vars(contentHash(*stmt)); }

		void visitFun(pst::Access<pst::Fun> stmt) override {
			// Signature only: name, parameters, return type. The body is covered by the
			// function's own Mod.
			addField(sigs, stmt->getName().illegalAccess().value()->unwrap().str());
			sigs(contentHash(*stmt->getParams().illegalAccess().value()));
			if (stmt->getRet().has_value())
				sigs(contentHash(*stmt->getRet()->illegalAccess().value()));
		}

		void visitNamespace(pst::Access<pst::Namespace> stmt) override {
			addField(structural, stmt->getName().illegalAccess().value()->unwrap().str());
			for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
				child.illegalAccess().value()->acceptVisitor(*this);
		}

		// Rewiring an import can change what every use-site resolves to.
		void visitImport(pst::Access<pst::Import> stmt) override { structural(contentHash(*stmt)); }
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
			program.items             = std::move(collector.items);
			program.fn_hashes         = std::move(collector.fn_hashes);
			program.link_names        = std::move(collector.link_names);
			program.global_inits      = std::move(collector.global_inits);
			program.const_inputs      = std::move(collector.const_inputs);
			program.sig_inputs        = std::move(collector.sig_inputs);
			program.class_inputs      = std::move(collector.class_inputs);
			program.global_var_inputs = std::move(collector.global_var_inputs);

			GlobalsHasher globals_hasher;
			for (const LoadedModule& module: program.modules) {
				// Module identity participates: moving a definition between files
				// changes link names even when content hashes stay equal.
				for (base::StrID component: module.module_path)
					addField(globals_hasher.structural, component.str());
				for_each_top_level(module, globals_hasher);
			}
			program.globals_hash_no_consts = globals_hasher.structural.finalize();
			program.global_defs_hash       = globals_hasher.vars.finalize();

			// The PSAC backend keeps the whole-world hash: fold every category back in.
			program.globals_hash = hashing::justHash<hashing::SHA256>(
				program.globals_hash_no_consts,
				globals_hasher.consts.finalize(),
				globals_hasher.sigs.finalize(),
				globals_hasher.classes.finalize(),
				globals_hasher.vars.finalize()
			);
		}

		return program;
	}

	std::string constInputKey(const sym::Symbol& symbol) {
		return "const:" + std::get<sym::ConstantSymbol>(symbol.kind).qualified_name.str();
	}

	std::string functionSigInputKey(const sym::Symbol& symbol) {
		return "sig:" + std::get<sym::FunctionSymbol>(symbol.kind).link_name.str();
	}

	std::string globalVarInputKey(const sym::Symbol& symbol) {
		return "gvar:" + std::get<sym::GlobalVariableSymbol>(symbol.kind).link_name.str();
	}

	std::string classLayoutInputKey(const tp::TypeInfo& type_info) {
		return "class:" + std::get<tp::Structure>(type_info.kind).qualified_name.str();
	}

	namespace {
		/// Namespace-qualified name of a type — TypeInfo::name alone is unqualified,
		/// so same-named types in different namespaces would alias in the hashes.
		std::string qualifiedTypeName(const Context& ctx, tp::TypeID type_id) {
			const tp::TypeInfo& info = ctx.getTypeInfo(type_id);

			std::string          name = info.name.str();
			const ns::Namespace* ns   = &ctx.getNamespace(info.namespace_id);
			while (ns->parent_namespace.has_value()) {
				name.insert(0, ".");
				name.insert(0, ns->name.str());
				ns = &ctx.getNamespace(*ns->parent_namespace);
			}
			return name;
		}
	}

	// Each input hash is prefixed with a kind tag so equal material of different
	// symbol kinds can never produce the same hash.
	base::Bit256 constInputHash(const Context& ctx, const sym::Symbol& symbol) {
		const auto& constant = std::get<sym::ConstantSymbol>(symbol.kind);

		hashing::StatefulHash<hashing::SHA256> hash;
		addField(hash, "const");
		addField(hash, constant.qualified_name.str());
		hash(constant.bits);
		addField(hash, qualifiedTypeName(ctx, symbol.type_id));
		return hash.finalize();
	}

	base::Bit256 functionSigInputHash(const Context& ctx, const sym::Symbol& symbol) {
		const auto& fn_type = std::get<tp::Function>(ctx.getTypeInfo(symbol.type_id).kind);

		hashing::StatefulHash<hashing::SHA256> hash;
		addField(hash, "sig");
		addField(hash, std::get<sym::FunctionSymbol>(symbol.kind).link_name.str());
		for (tp::TypeID param_type_id: fn_type.param_types)
			addField(hash, qualifiedTypeName(ctx, param_type_id));
		addField(hash, qualifiedTypeName(ctx, fn_type.return_type));
		return hash.finalize();
	}

	base::Bit256 globalVarInputHash(const Context& ctx, const sym::Symbol& symbol) {
		hashing::StatefulHash<hashing::SHA256> hash;
		addField(hash, "gvar");
		addField(hash, std::get<sym::GlobalVariableSymbol>(symbol.kind).link_name.str());
		addField(hash, qualifiedTypeName(ctx, symbol.type_id));
		return hash.finalize();
	}

	base::Bit256 classLayoutInputHash(const Context& ctx, const tp::TypeInfo& type_info) {
		hashing::StatefulHash<hashing::SHA256> hash;
		addField(hash, "class");
		addField(hash, std::get<tp::Structure>(type_info.kind).qualified_name.str());
		for (sym::SymbolID field_sym_id: std::get<tp::Structure>(type_info.kind).field_symbols) {
			const sym::Symbol& field_sym = ctx.getSymbol(field_sym_id);
			addField(hash, field_sym.name.str());
			addField(hash, qualifiedTypeName(ctx, field_sym.type_id));
		}
		return hash.finalize();
	}

	std::string compileOneFunctionToBytes(
		const Context& base_ctx, const FunctionWorkItem& item, SymbolUseObserver* observer
	) {
		Context ctx             = base_ctx;
		ctx.symbol_use_observer = observer;
		LlvmProgramModule   module;
		LlvmLoweringContext lowering(module);
		FunctionPass        pass(ctx, module, lowering);

		if (item.enclosing_ns != ctx.root_ns_id) ctx.enterNamespace(item.enclosing_ns);
		pass.visitFun(item.stmt);

		module.verify();
		return module.emitObjectBytes();
	}

	void compileOneFunction(
		const Context&               base_ctx,
		const FunctionWorkItem&      item,
		const std::filesystem::path& object_path,
		SymbolUseObserver*           observer
	) {
		writeFileBytes(object_path, compileOneFunctionToBytes(base_ctx, item, observer));
	}

	std::string emitGlobalsObjectBytes(
		const Context&                     base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
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
		return module.emitObjectBytes();
	}

	void emitGlobalsObject(
		const Context&                     base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
		const std::filesystem::path&       object_path,
		SymbolUseObserver*                 observer
	) {
		writeFileBytes(object_path, emitGlobalsObjectBytes(base_ctx, global_inits, observer));
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
