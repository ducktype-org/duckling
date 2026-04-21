#pragma once

#include <driver_private/lir_unit_with_name.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context_fd.hpp>

#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace compiler::repl {

	/**
	 * @brief Driver view over one logical script, hiding the internal statement chain.
	 *
	 * ScriptModule points at a package script node; statement modules live in `ScriptData` on that
	 * node (populated by `buildScriptModuleFromTreeNode`).
	 *
	 * `scriptID()` names the merged LIR/DVM artifact derived from the script module hash.
	 */
	class ScriptModule final {
	public:
		/** @brief View over a script node already present in the package module tree. */
		ScriptModule(base::StrID script_id, frontend::ModuleID script_module_id);

		[[nodiscard]]
		base::StrID scriptID() const {
			return m_script_id;
		}

		[[nodiscard]]
		bool empty() const;

		[[nodiscard]]
		std::span<const base::Ref<frontend::ModuleTree>> statementModules() const;

		[[nodiscard]]
		frontend::ModuleID terminalModuleID() const;

		[[nodiscard]]
		frontend::ModuleID scriptModuleID() const {
			return m_script_module_id;
		}

	private:
		base::StrID        m_script_id;
		frontend::ModuleID m_script_module_id;
	};

	/**
	 * @brief Build or refresh the statement chain for a package script node.
	 *
	 * Reads the `.ds` script source from the tree, splits it into statements, anchors the first
	 * statement for import resolution, and stores the chain on the script node.
	 */
	std::expected<ScriptModule, std::string> buildScriptModuleFromTreeNode(
		query::Context& ctx, frontend::ModuleID script_module_id
	);

	/**
	 * @brief Append all functions and globals from one LIR module chunk into another.
	 *
	 * Script compilation lowers each statement into a chunk and then merges those
	 * chunks into a single synthetic module. This helper performs that merge step.
	 */
	void appendScriptLIRModuleData(
		driver::LIRUnitWithBackendName& merged, const driver::LIRUnitWithBackendName& chunk
	);

}  // namespace compiler::repl
