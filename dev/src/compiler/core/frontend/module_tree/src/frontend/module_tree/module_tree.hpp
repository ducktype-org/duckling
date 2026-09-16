#pragma once

#include "access.hpp"
#include "module_id.hpp"
#include "source_file.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <filesystem/file.hpp>
#include <hashing/component_hash.hpp>

#include <functional>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>

namespace compiler::frontend::packages {
	class PackageAccessLocked;
}

namespace compiler::frontend {
	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a single file module.
	 */
	constexpr std::string_view LANG_MODULE_FILE = ".dk";

	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a single file script.
	 */
	constexpr std::string_view LANG_SCRIPT_FILE = ".dks";

	// Regexes to reject files/directories starting with '.' or '$'
	/**
	 * @brief Turns a path found while walking the disk into the file the compiler should read.
	 *
	 * The default reads the path itself; the language server substitutes an editor buffer for
	 * the files it holds open.
	 */
	using FileResolver = std::function<fs::File(const fs::FilePath& disk_path)>;

	/**
	 * @brief The resolver that simply reads what is on disk.
	 */
	const FileResolver& identityFileResolver();

	const std::regex DEFAULT_REJECT_FILE_REGEX      = std::regex(R"((\$.*|\..*))");
	const std::regex DEFAULT_REJECT_DIRECTORY_REGEX = std::regex(R"((\$.*|\..*))");

	class ModuleTreeBuilder;
	class ModuleTreeModifier;
	struct GetModuleID_Functor;

	/**
	 * @brief REPL-specific data structure.
	 *
	 * Only used for repl modules.
	 */
	struct ReplData final {
		/**
		 * Parent REPL module in chronological order.
		 * Optional - only empty for first REPL module.
		 */
		base::Optional<ModuleID> m_repl_module_parent;
	};

	/**
	 * @brief Represents a single module in the Duckling project tree.
	 *
	 * ModuleTree provides a hierarchical, in-memory representation of a module,
	 * including its source files, submodules, and other files.
	 * The ModuleTree is the first instance of module in duckling compiling process
	 * the main use case is to build a module tree form existing folder, and then
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
	 *
	 * \parallel note that compiler::frontend::SourceFile::getComponentHash /
	 * compiler::frontend::SourceFile::invalidateComponentHash are lazy-initialized per-module
	 * component hash. Lazy writes can race
	 * under concurrency.
	 */
	class ModuleTree final {
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend struct GetModuleID_Functor;
		friend struct ModuleID;

	public:
		ModuleID getModuleID() const;

		/**
		 * Accessor to module's parent module. A module might not have a parent module.
		 * @return If a module has parent module, then a reference to it is passed
		 * inside the base::Optional.
		 */
		[[nodiscard]]
		base::Optional<ModuleAccessLocked> getParentModule() const;

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
		FileAccessLocked getMainSourceFile() const;

		/**
		 * Accesses the submodules located in this module.
		 * @note Use this only if you need all submodules. For single submodule access, use
		 * getSubmoduleByName().
		 * @return A lazy view that can be unlocked within a query context or accessed illegally
		 * (outside queries).
		 */
		[[nodiscard]]
		SubmodulesAccessLocked getSubmodules() const;

		/**
		 * Access a single submodule edge by name.
		 * Registers dependency via QueryModuleChildSideInput when unlocked.
		 * Use this function in lookups when you need only a submodule with some name.
		 * @param name Name of the submodule to access.
		 * @return AccessLocked wrapper that may contain the submodule if it exists.
		 */
		[[nodiscard]]
		ModuleChildAccessLocked getSubmoduleByName(base::StrID name) const;

		/**
		 * Accesses all the other files that are located inside the module.
		 * @return A base::HashMap that maps a file extension to a vector
		 * with files with this extension.
		 */
		[[nodiscard]]
		const base::HashMap<base::StrID, std::vector<fs::File>>& getOtherFiles() const;

		/**
		 * Parses the name of the module.
		 * @return base::StrID with the name. `A.dk -> A`, `/.../module/ -> module`.
		 */
		[[nodiscard]]
		base::StrID getName() const;

		/**
		 * @brief Access the package this module belongs to.
		 * Available for every module (not only root). On unlock registers
		 * QueryPackageSideInput dependency on the owning package; outside of queries use
		 * illegalAccess to obtain the raw package id.
		 */
		[[nodiscard]] packages::PackageAccessLocked getPackage() const;

		/**
		 * Check if this module is a REPL-generated module.
		 * REPL modules have special cross-module lookup behavior.
		 * @return true if this is a REPL module, false otherwise
		 */
		[[nodiscard]]
		bool isReplModule() const {
			return m_repl_data.has_value();
		}

		/**
		 * Get the parent REPL module.
		 * Only valid for REPL modules.
		 * @return ModuleID of the parent REPL module, or empty if this is the first REPL module
		 */
		[[nodiscard]]
		base::Optional<ModuleID> getReplModuleParent() const {
			CORE_ASSERT(
				m_repl_data.has_value(), "repl data of a node with parent should have value!"
			);
			return m_repl_data->m_repl_module_parent;
		}

		/**
		 * Returns the ComponentHash of the module.
		 * It is calculated from the module logical path.
		 * For example, for a module tree like:
		 * /root
		 *   /sub1
		 *     /sub2
		 * The component hash of sub2 will be ComponentHash({"root", "sub1", "sub2"})
		 * @param module_id ModuleID of the module to get the component hash for.
		 * @note This function is thread-safe only if the module tree hash is not modified/deleted
		 * concurrently.
		 */
		[[nodiscard]]
		static const hashing::ComponentHash& getPathComponentHash(ModuleID module_id);

		/**
		 * Returns the stable hash of the module.
		 * This is the proper hash of the module to use in SideInputs.
		 * @param module_id ModuleID of the module to get the hash for.
		 */
		[[nodiscard]]
		static const hashing::ComponentHash::HashType& getModuleHash(ModuleID module_id);

		/**
		 * Builds a dotted, human-readable identification of this module, walking up from the
		 * package ID at the root down to this module.
		 * For a module tree like:
		 * /root
		 *   /sub1
		 *     /sub2
		 * belonging to package `pkg`, `sub2` identifies as `pkg.root.sub1.sub2`.
		 * @note This is meant for logging and diagnostics only, but it's a stable identifier.
		 * @param ctx Query context, used to unlock the access to the parent module.
		 */
		[[nodiscard]]
		std::string humanReadableID(query::Context& ctx) const;

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


		/**
		 * Invalidate current module hash and component hash, used when module structure changes
		 * This also invalidates all children modules recursively
		 * @note This is not thread-safe, this should be called in main thread only with no active
		 * workers.
		 */
		void invalidateHash();

		/**
		 * Use a parent component hash, and update module hash and path component hash for this module
		 * only This does not propagate to children
		 * still query invalidation needs to be added
		 */
		void updateModuleHash();

		/**
		 * Updates the module hashes from the root module down to this module.
		 * This is needed to ensure that all parent modules have their hashes updated before this
		 * module.
		 * @note This function updates both module hash and path component hash.
		 */
		void updateModuleHashFromRootToThis();

		/**
		 * @brief Remove ModuleTree from static storage.
		 * @note This will invalidate all references!
		 * In principle it should only be used in ModuleTreeModifier in pair with query invalidations.
		 */
		static void removeModuleFromStorage(base::Ref<ModuleTree> module);

		/**
		 * Ensures a ModuleTree reference still points to a tracked instance during development
		 * builds.
		 */
		static void checkDanglingReference(const base::Ref<ModuleTree>& candidate);

		// this is a self pointer, it is necessary to get the ModuleID from the const ModuleTree
		base::Optional<ModuleID> m_id;

		base::StrID m_name;

		base::Optional<base::Ref<ModuleTree>> m_parent;

		base::Optional<base::Ref<SourceFile>>             m_main_source_file;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>>
			m_other_files;  //< Other files in the module (not SourceFiles) currently nothing is
		                    // happening with them. Do not use this in query unless AccessLocked is
		                    // implemented for this

		base::Optional<usize> m_storage_handle;  //< Key to support removal from static storage

		base::Optional<hashing::ComponentHash>
			m_path_component_hash;  //< ComponentHash of the module's logical path: eg
		                            // package_name/root/submodule1/sub2
		base::Optional<hashing::ComponentHash::HashType>
			m_hash;                 //< This is the actual hash for the Module used in SideInput
		/**
		 * @brief Synchronizes lazy module hash/path-hash recomputation for this module.
		 * @note Hold this lock while reading/writing m_path_component_hash or m_hash during lazy
		 * recomputation flow (updateModuleHashFromRootToThis/updateModuleHash).
		 */
		mutable base::Box<std::mutex> m_hash_recompute_mutex;

		/**
		 * Package ID associated with this module tree.
		 * Used for component hash calculation.
		 */
		base::StrID m_package_id;

		/**
		 * REPL-specific data.
		 * Optional - only set for modules created in REPL sessions.
		 * Presence of this optional indicates the module is a REPL module.
		 */
		base::Optional<ReplData> m_repl_data;
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
		 * Creates a new builder instance with a random package ID.
		 * This is used for testing purposes.
		 * @return Boxed ModuleTreeBuilder.
		 */
		static base::Box<ModuleTreeBuilder> createWithRandomPackageID();

		/**
		 * Factory method to create ModuleTree from filesystem tree.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> create(
			const fs::File&     root,
			base::StrID         package_id,
			const std::regex&   file_reject   = DEFAULT_REJECT_FILE_REGEX,
			const std::regex&   dir_reject    = DEFAULT_REJECT_DIRECTORY_REGEX,
			const FileResolver& file_resolver = identityFileResolver()
		);

		/**
		 * Factory method to create ModuleTree from filesystem tree with random package ID.
		 * This is used for testing purposes.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> createWithRandomPackageID(
			const fs::File&   root,
			const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX,
			const std::regex& dir_reject  = DEFAULT_REJECT_DIRECTORY_REGEX
		);

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
		 * Sets the package ID for the module tree.
		 * The package ID must be set for every module tree
		 * @param package_id The package ID to set.
		 */
		void setPackageID(base::StrID package_id);

		/**
		 * Sets the parent module.
		 * @param parent The parent module.
		 */
		void setParent(base::Ref<ModuleTree> parent);

		/**
		 * Sets REPL-specific module data.
		 * @param repl_data The ReplData struct.
		 */
		void setReplModule(const ReplData& repl_data);

		/**
		 * Builds the module tree from a single file (single-file module).
		 * @param file The file to build from.
		 */
		void buildFromSingleFile(const fs::File& file, base::StrID package_id);

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
			const fs::File&     directory,
			base::StrID         package_id,
			const std::regex&   file_reject   = DEFAULT_REJECT_FILE_REGEX,
			const std::regex&   dir_reject    = DEFAULT_REJECT_DIRECTORY_REGEX,
			const FileResolver& file_resolver = identityFileResolver()
		);

		/**
		 * Handles a new file found during directory traversal.
		 * This is a helper function used when creating module tree from fs::File.
		 * @param file The file to handle.
		 */
		void handleNewFile(const fs::File& file);

		base::Optional<base::Ref<ModuleTree>>             m_parent;
		base::Optional<fs::File>                          m_main_source_file_path;
		base::StrID                                       m_package_id;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;

		base::StrID m_name;
		bool        m_finalized;

		base::Optional<ReplData> m_repl_data;
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
		 * Changes the package ID of the given module and ALL its submodules recursively.
		 * All modules in the same module tree must have the same package ID.
		 * @note This can only be done on root modules (modules without a parent).
		 * @param module The module to modify.
		 * @param new_package_id The new package ID to set.
		 */
		static void changePackageID(base::Ref<ModuleTree> module, base::StrID new_package_id);

		/**
		 * Removes the module with the given ModuleID from the module map.
		 * Also removes it from its parent's submodules and deletes associated source files.
		 * @param module_id The ModuleID to remove.
		 * This will set the parent of all submodules to the parent of the removed module.
		 */
		static void removeSingleModule(base::Ref<ModuleTree> module);

		/**
		 * Removes the given module and all of its submodules recursively.
		 * Parent hashes are updated once after the entire subtree is removed.
		 * @param module_id The ModuleID to remove.
		 */
		static void removeModuleRecursive(base::Ref<ModuleTree> module);


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
	 * Creates a completely new module tree from the given file and returns the ModuleID
	 * created ModuleTree contains independent submodules, source files and PSTs
	 * it is created recursively based on the Duckling module structure
	 * @param file File representing the root of the module tree
	 * @param package_id The package ID to associate with the module tree
	 * for more details see ModuleTreeBuilder::create
	 */
	ModuleID createModuleTree(const fs::File& file, base::StrID package_id);

	/**
	 * @brief Creates a module tree by walking the real filesystem, with every file it finds
	 * passed through `file_resolver` first.
	 *
	 * @param root File representing the root of the module tree
	 * @param package_id The package ID to associate with the module tree
	 * @param file_resolver Substitutes the file the compiler reads for a given path
	 * @return The ModuleID of the created module tree
	 */
	ModuleID createModuleTreeFromFS(
		const fs::File&     root,
		base::StrID         package_id,
		const FileResolver& file_resolver,
		const std::regex&   file_reject = DEFAULT_REJECT_FILE_REGEX,
		const std::regex&   dir_reject  = DEFAULT_REJECT_DIRECTORY_REGEX
	);

	/**
	 * @brief: Concurrently parses all source files in the module tree and their submodules
	 * recursively, creating PSTs for each file. This function should be called before collecting
	 * Inputs from the previous compilation graph.
	 *
	 * Blocks until all files in the module tree have been parsed.
	 *
	 * @param module_id The ModuleID of the root module to start parsing from
	 * @note This function cannot be called from query
	 */
	void parseAllFilesInModuleTree(ModuleID module_id);

	/*
	 * Creates a completely new module tree with a random package ID from the given file.
	 * @note This is used mostly for tests.
	 * @param file File representing the root of the module tree
	 * @return The ModuleID of the created module tree
	 */
	ModuleID createModuleTreeWithRandomPackageID(const fs::File& file);

	/**
	 * Creates a module tree from a string containing source code contents.
	 * This is primarily used for REPL sessions and testing.
	 * Creates a virtual file from the provided contents and sets it as the main source file.
	 * If no package_id is provided, a random one is generated.
	 * @param contents The source code content as a string
	 * @param package_id Optional package ID; if empty, a random one is generated
	 * @return The ModuleID of the created module tree
	 */
	ModuleID createModuleTreeFromContents(
		std::string_view contents, base::Optional<base::StrID> package_id = {}
	);
}
