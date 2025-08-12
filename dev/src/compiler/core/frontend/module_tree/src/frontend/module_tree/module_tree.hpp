#pragma once

#include "file_id.hpp"
#include "module_id.hpp"
#include "source_file.hpp"

#include <pst_parser/pst.hpp>

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>

#include <filesystem/file.hpp>

#include <regex>
#include <string>

namespace compiler::frontend {
	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a source file of the module.
	 */
	constexpr std::string_view LANG_SOURCE_FILE = ".duck";

	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a single file module.
	 */
	constexpr std::string_view LANG_MODULE_FILE = ".dmf";

	// Regexes to reject files/directories starting with '.' or '$'
	const std::regex DEFAULT_REJECT_FILE_REGEX      = std::regex(R"((\$.*|\..*))");
	const std::regex DEFAULT_REJECT_DIRECTORY_REGEX = std::regex(R"((\$.*|\..*))");

	class ModuleTreeBuilder;
	class ModuleTreeModifier;

	class ModuleTree {
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;

		static base::StableHashMap<ModuleID, ModuleTree> module_map;

	public:
		/**
		 * Returns a reference to the ModuleTree with the given ModuleID.
		 * Asserts if the module does not exist.
		 */
		static Ref<ModuleTree> getModule(ModuleID id);

		/**
		 * Accessor to module's parent module. A module might not have a parent module.
		 * @return If a module has parent module, then a reference to it is passed
		 * inside the base::Optional.
		 */
		[[nodiscard]]
		base::Optional<base::CRef<ModuleTree>> getParentModule() const;

		/**
		 * Checks if a module contains main source file.
		 * @return True if the main source file exists, false otherwise.
		 */
		[[nodiscard]]
		bool hasMainSourceFile() const;

		/**
		 * Accesses the main source file of the module.
		 * If a pointer to file is invalid, then throws an std::logic_error exception.
		 * @return A reference to the main source file.
		 */
		[[nodiscard]]
		base::CRef<SourceFile> getMainSourceFile() const;

		/**
		 * Accesses the source files of the module.
		 * @return A const reference to a vector of SourceFile references.
		 */
		[[nodiscard]]
		const std::vector<base::Ref<SourceFile>>& getSourceFiles() const;

		/**
		 * Accesses the submodules located in this module. Submodules are indexed by their name.
		 * @return base::HashMap that maps a name of the submodule to the pointer to the submodule.
		 */
		[[nodiscard]]
		const base::HashMap<base::StrID, base::Ref<ModuleTree>>& getSubmodules() const;

		/**
		 * Accesses all the other files that are located inside the module.
		 * @return A base::HashMap that maps a file extension to a vector
		 * with files with this extension.
		 */
		[[nodiscard]]
		const base::HashMap<base::StrID, std::vector<fs::File>>& getOtherFiles() const;

		/**
		 * Parses the name of the module.
		 * @return base::StrID with the name. `A.dmf -> A`, `/.../module/ -> module`.
		 */
		[[nodiscard]]
		base::StrID getName() const;

		/**
		 * Creates a nice, human-readable representation of this module tree.
		 * @param indentation For regular printing, leave 0.
		 * @return std::string with the representation.
		 */
		std::string prettyPrint(u32 indentation = 0) const;

		/**
		 * Fetches the id of the module.
		 * @return compiler::frontend::ModuleID.
		 */
		[[nodiscard]]
		ModuleID getID() const;

		// ModuleTree(const ModuleTree&) = delete;
		// ModuleTree& operator=(const ModuleTree&) = delete;

	private:
		/**
		 * Constructs a ModuleTree with a new unique ModuleID.
		 */
		ModuleTree();


		ModuleID m_id;

		base::StrID m_name;

		base::Optional<base::Ref<ModuleTree>> m_parent;

		base::Optional<base::Ref<SourceFile>>             m_main_source_file;
		std::vector<base::Ref<SourceFile>>                m_source_files;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;
	};

	/**
	 * ModuleTreeBuilder - Builder class for constructing ModuleTree instances.
	 *
	 * Allows step-by-step construction of module trees with validation.
	 */
	class ModuleTreeBuilder {
		friend class base::Box<ModuleTreeBuilder>;
		friend base::Box<ModuleTreeBuilder> base::makeBox<ModuleTreeBuilder>();

	public:
		/**
		 * Creates a new builder instance.
		 * @return Boxed ModuleTreeBuilder.
		 */
		static base::Box<ModuleTreeBuilder> create();

		/**
		 * Factory method to create ModuleTree from filesystem tree.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> create(
			const fs::File&   root,
			const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX,
			const std::regex& dir_reject  = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * Adds a source file to the module being built.
		 * @param file The source file to add.
		 */
		void addSourceFile(const fs::File& file);

		/**
		 * Sets the main source file for the module.
		 * @param file The main source file.
		 */
		void setMainSourceFile(const fs::File& file);

		/**
		 * Adds a submodule to the module being built.
		 * @param submodule The submodule to add.
		 */
		void addSubmodule(base::Ref<ModuleTree> submodule);

		/**
		 * Adds an other file to the module being built.
		 * @param file The file to add.
		 */
		void addOtherFile(const fs::File& file);

		/**
		 * Sets the name of the module.
		 * @param name The name to set.
		 */
		void setName(base::StrID name);

		/**
		 * Sets the parent module.
		 * @param parent The parent module.
		 */
		void setParent(base::Ref<ModuleTree> parent);

		/**
		 * Builds the module tree from a directory structure.
		 * This will recursively traverse the directory and build the module tree.
		 * @param directory The root directory to build the module tree from.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 */
		void buildFromDirectory(
			const fs::File&   directory,
			const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX,
			const std::regex& dir_reject  = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * Builds the module tree from a single file (single-file module).
		 * @param file The file to build from.
		 */
		void buildFromSingleFile(const fs::File& file);

		/**
		 * Validates the current state of the builder.
		 * @return True if valid (not finalized), false otherwise.
		 */
		[[nodiscard]]
		bool isValid() const;

		/**
		 * Finalizes the construction and returns the built ModuleTree.
		 * After calling this, the builder becomes invalid.
		 * @return The constructed ModuleTree.
		 */
		base::Ref<ModuleTree> finalize();

	private:
		/**
		 * Constructs a ModuleTreeBuilder.
		 */
		ModuleTreeBuilder();

		/**
		 * Handles a new file found during directory traversal.
		 * @param file The file to handle.
		 */
		void handleNewFile(const fs::File& file);

		/**
		 * Checks if a file name is valid according to the reject regex.
		 * @param filename The file name to check.
		 * @param reject_file_regex The regex to use for rejection.
		 * @return True if valid, false otherwise.
		 */
		static bool isFileNameValid(const std::string& filename, const std::regex& reject_file_regex);

		/**
		 * Checks if a directory name is valid according to the reject regex.
		 * @param dirname The directory name to check.
		 * @param reject_directory_regex The regex to use for rejection.
		 * @return True if valid, false otherwise.
		 */
		static bool isDirectoryNameValid(
			const std::string& dirname, const std::regex& reject_directory_regex
		);

		base::Optional<base::Ref<ModuleTree>>             m_parent;
		base::Optional<fs::File>                          m_main_source_file_path;
		std::vector<fs::File>                             m_source_file_paths;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;

		base::StrID m_name;
		bool        m_finalized;
	};

	class ModuleTreeModifier final {
	public:
		/**
		 * Adds a source file to the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 * @param file The file to add.
		 */
		static void addSourceFile(
			query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file
		);

		/**
		 * Removes a source file from its module by FileID.
		 * @param ctx Query context.
		 * @param file_id The FileID to remove.
		 */
		static void removeSourceFile(query::Context& ctx, FileID file_id);

		/**
		 * Sets the main source file for the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 * @param file The file to set as main source file.
		 */
		static void setMainSourceFile(
			query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file
		);

		/**
		 * Removes the main source file from the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 */
		static void removeMainSourceFile(query::Context& ctx, base::Ref<ModuleTree> module);

		/**
		 * Adds a submodule to the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 * @param submodule The submodule to add.
		 */
		static void addSubmodule(
			query::Context& ctx, base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule
		);

		/**
		 * Adds an "other" file to the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 * @param file The file to add.
		 */
		static void addOtherFile(
			query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file
		);

		/**
		 * Removes an "other" file from the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 * @param file The file to remove.
		 */
		static void removeOtherFile(
			query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file
		);

		/**
		 * Sets the parent of the given module.
		 * If parent is set, adds this module as a submodule to the parent.
		 * If parent is not set, removes the current parent.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 * @param parent The new parent module (optional).
		 */
		static void setParent(
			query::Context&                       ctx,
			base::Ref<ModuleTree>                 module,
			base::Optional<base::Ref<ModuleTree>> parent
		);

		/**
		 * Removes the parent from the given module.
		 * @param ctx Query context.
		 * @param module The module to modify.
		 */
		static void removeParent(query::Context& ctx, base::Ref<ModuleTree> module);

		/**
		 * Removes the module with the given ModuleID from the module map.
		 * Also removes it from its parent's submodules and deletes associated source files.
		 * @param ctx Query context.
		 * @param module_id The ModuleID to remove.
		 */
		static void removeModule(query::Context& ctx, ModuleID module_id);

		/**
		 * Notifies that a file has been modified and updates its SourceFile.
		 * @param ctx Query context.
		 * @param file The file that was modified.
		 */
		static void fileModified(query::Context& ctx, const fs::File& file);

	private:
		/**
		 * Private constructor to prevent instantiation.
		 */
		ModuleTreeModifier() = default;
	};
}
