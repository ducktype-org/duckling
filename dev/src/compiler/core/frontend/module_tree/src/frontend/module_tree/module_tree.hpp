#pragma once

#include "file_id.hpp"
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

	/**
	 * @brief Represents a single module in the Duckling project tree.
	 *
	 * ModuleTree provides a hierarchical, in-memory representation of a module,
	 * including its source files, submodules, and other files.
	 * The ModuleTree is the first instance of module in duckling compiling process
	 * the main use case is to build a module tree form exesting folder, and then
	 * extract the pst from source files
	 * But module tree can be also created manually.
	 *
	 * - Tracks main source file, additional source files, submodules, and other files.
	 * - Supports pretty-printing for debugging and inspection.
	 * - Immutable after construction; use ModuleTreeModifier for changes.
	 * - Submodules form a tree structure, each with a parent reference.
	 * - Source files and other files can have any path, including outside the module directory.
	 *   Files may be virtual or real; their location on disk does not affect their association
	 *   with the module.
	 */
	class ModuleTree final {
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;

	public:
		ModuleID getModuleID() const;

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
		 * Does not contain Main module file (Main source file)
		 * @return A const reference to a vector of SourceFile references
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

		ModuleTree(const ModuleTree&)            = delete;
		ModuleTree& operator=(const ModuleTree&) = delete;
		ModuleTree(ModuleTree&&) noexcept        = default;

	private:
		ModuleTree();

		base::Optional<ModuleID> m_id;

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
	 * Allows step-by-step construction of module trees with assertions that check the correctness
	 * of module creation.
	 */
	class ModuleTreeBuilder final {
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
		 * Builds the module tree from a single file (single-file module).
		 * @param file The file to build from.
		 */
		void buildFromSingleFile(const fs::File& file);

		/**
		 * Checks if the builder is finalized.
		 * @return True if finalized, false otherwise.
		 */
		[[nodiscard]]
		bool isFinalized() const;

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
		 * Handles a new file found during directory traversal.
		 * @param file The file to handle.
		 */
		void handleNewFile(const fs::File& file);

		base::Optional<base::Ref<ModuleTree>>             m_parent;
		base::Optional<fs::File>                          m_main_source_file_path;
		std::vector<fs::File>                             m_source_file_paths;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;

		base::StrID m_name;
		bool        m_finalized;
	};

	/**
	 * @brief Modifier class for making changes to ModuleTree instances.
	 *
	 * ModuleTreeModifier provides static methods to add, remove, and update source files,
	 * submodules, parent relationships, and other files within a ModuleTree.
	 * All modifications are performed in-place and require a query context.
	 * This class cannot be instantiated.
	 * If you are using this class you should know what you are doing.
	 */
	class ModuleTreeModifier final {
	public:
		/**
		 * Adds a source file to the given module.
		 * @param module The module to modify.
		 * @param file The file to add.
		 */
		static void addSourceFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Removes a source file from its module.
		 * @param file The SourceFile to remove.
		 */
		static void removeSourceFile(base::Ref<SourceFile> file);

		/**
		 * Sets the main source file for the given module.
		 * @param module The module to modify.
		 * @param file The file to set as main source file.
		 */
		static void setMainSourceFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Removes the main source file from the given module.
		 * @param module The module to modify.
		 */
		static void removeMainSourceFile(base::Ref<ModuleTree> module);

		/**
		 * Adds a submodule to the given module.
		 * @param module The module to modify.
		 * @param submodule The submodule to add.
		 */
		static void addSubmodule(base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule);

		/**
		 * Adds an "other" file to the given module.
		 * @param module The module to modify.
		 * @param file The file to add.
		 */
		static void addOtherFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Removes an "other" file from the given module.
		 * @param module The module to modify.
		 * @param file The file to remove.
		 */
		static void removeOtherFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Sets the parent of the given module.
		 * If parent is set, adds this module as a submodule to the parent.
		 * If parent is not set, removes the current parent.
		 * @param module The module to modify.
		 * @param parent The new parent module (optional).
		 */
		static void setParent(
			base::Ref<ModuleTree> module, base::Optional<base::Ref<ModuleTree>> parent
		);

		/**
		 * Removes the parent from the given module.
		 * @param module The module to modify.
		 */
		static void removeParent(base::Ref<ModuleTree> module);

		/**
		 * Removes the module with the given ModuleID from the module map.
		 * Also removes it from its parent's submodules and deletes associated source files.
		 * @param module_id The ModuleID to remove.
		 */
		static void removeModule(base::Ref<ModuleTree> module);

		/**
		 * Notifies that a file has been modified and updates its SourceFile.
		 * @param file The file that was modified.
		 */
		static void fileModified(const fs::File& file);

	private:
		/**
		 * Private constructor to prevent instantiation.
		 */
		ModuleTreeModifier() = default;
	};

	/*
	 * Creates a new module tree from the given file and returns the ModuleID
	 */
	ModuleID createModuleTree(const fs::File& file);

}
