#include "script_helpers.hpp"

#include <driver/repl_utils/repl_split_helpers.hpp>
#include <driver/repl_utils/repl_statement_helpers.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/packages/access.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/vector_utils.hpp>
#include <base/str/str_utils.hpp>

namespace compiler::repl {
	namespace {
		/** Stable artifact name derived from the script node's package module hash. */
		base::StrID scriptIDFromModule(frontend::ModuleID script_module_id) {
			return base::StrID(
				base::strConcat(
					"script_module_",
					frontend::ModuleTree::getModuleHash(script_module_id).toStringHex()
				)
					.c_str()
			);
		}

		/**
		 * Splits script source and builds the REPL parent-linked statement chain for script runs.
		 */
		std::expected<std::vector<base::Ref<frontend::ModuleTree>>, std::string> buildStatementModulesFromSource(
			query::Context&                           ctx,
			std::string_view                          source,
			const base::Optional<frontend::ModuleID>& initial_parent_module,
			std::string_view                          module_name_prefix,
			const base::Optional<base::StrID>&        package_id
		) {
			auto split_result = repl::splitInputIntoStatements(ctx, source);
			if (!split_result.has_value())
				return std::unexpected(
					base::strConcat("Script parsing failed: ", split_result.error())
				);

			std::vector<base::Ref<frontend::ModuleTree>> modules;
			base::Optional<frontend::ModuleID>           parent_module_id;
			u64                                          statement_counter = 0;

			if (initial_parent_module.has_value()) parent_module_id = initial_parent_module.value();

			// This is very ugly, but for now works.
			auto implicit_builtins_module = repl::createSyntheticChainedStatementModule(
				"import core.builtins.*;",
				parent_module_id,
				statement_counter,
				module_name_prefix,
				package_id
			);
			parent_module_id = implicit_builtins_module->getModuleID();
			modules.push_back(implicit_builtins_module);
			++statement_counter;

			if (split_result->empty()) {
				modules.push_back(repl::createSyntheticChainedStatementModule(
					"", parent_module_id, statement_counter, module_name_prefix, package_id
				));
				return modules;
			}

			for (const auto& statement_source: *split_result) {
				auto module_ref = repl::createSyntheticChainedStatementModule(
					statement_source,
					parent_module_id,
					statement_counter,
					module_name_prefix,
					package_id
				);
				parent_module_id = module_ref->getModuleID();
				modules.push_back(module_ref);
				++statement_counter;
			}

			return modules;
		}
	}  // namespace

	ScriptModule::ScriptModule(base::StrID script_id, frontend::ModuleID script_module_id):
		  m_script_id(script_id),
		  m_script_module_id(script_module_id) {}

	bool ScriptModule::empty() const {
		return frontend::getModuleRef(m_script_module_id)->getScriptStatementModules().empty();
	}

	std::span<const base::Ref<frontend::ModuleTree>> ScriptModule::statementModules() const {
		return frontend::getModuleRef(m_script_module_id)->getScriptStatementModules();
	}

	frontend::ModuleID ScriptModule::terminalModuleID() const {
		const auto modules = statementModules();
		CORE_ASSERT(!modules.empty(), "ScriptModule has no statement modules");
		return modules.back()->getModuleID();
	}

	std::expected<ScriptModule, std::string> buildScriptModuleFromTreeNode(
		query::Context& ctx, frontend::ModuleID script_module_id
	) {
		auto script_tree
			= frontend::GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				script_module_id
			);
		if (!script_tree->isScriptModule())
			return std::unexpected("Module is not a package script node");

		if (!script_tree->hasScriptSourceFile())
			return std::unexpected("Script module is missing its .ds source file");

		// First statement's REPL parent: enclosing package module (for `import sub`, etc.).
		// Script-only package roots still act as import anchors.
		base::Optional<frontend::ModuleID> import_parent;
		if (auto parent = script_tree->getParentModule()) {
			const auto parent_module = frontend::getModuleRef(parent->illegalAccess().getID());
			if (!parent_module->isScriptModule()) import_parent = parent->illegalAccess().getID();
		}
		auto script_package_id = script_tree->getPackage().illegalAccess().getID();

		auto script_source
			= frontend::getFileRef(script_tree->getScriptSourceFile().illegalAccess().getID())
		          ->getFileIllegalAccess()
		          .getContent()
		          .view()
		          .stdString();

		auto statement_modules_result = buildStatementModulesFromSource(
			ctx, script_source, import_parent, "script_", script_package_id
		);
		if (!statement_modules_result.has_value())
			return std::unexpected(statement_modules_result.error());

		frontend::ModuleTreeModifier::setScriptStatementModules(
			script_tree, std::move(statement_modules_result.value())
		);

		return ScriptModule(scriptIDFromModule(script_module_id), script_module_id);
	}

	void appendScriptLIRModuleData(
		driver::LIRUnitWithBackendName& merged, const driver::LIRUnitWithBackendName& chunk
	) {
		base::appendToVector(merged.lir_unit.lir_functions, chunk.lir_unit.lir_functions);
		base::appendToVector(merged.lir_unit.lir_globals, chunk.lir_unit.lir_globals);
	}

}  // namespace compiler::repl
