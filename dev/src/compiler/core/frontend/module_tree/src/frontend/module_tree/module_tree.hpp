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
#include <vector>

namespace compiler::frontend::packages {
	class PackageAccessLocked;
}

namespace compiler::frontend {
	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a module file.
	 */
	constexpr std::string_view LANG_MODULE_FILE = ".dk";

	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a script file.
	 */
	constexpr std::string_view LANG_SCRIPT_FILE = ".dks";

	/**
	 * @brief Turns a file found while walking the disk into the file the compiler should read.
	 *
	 * The default hands back the file itself; the language server substitutes an editor buffer
	 * for the files it holds open.
	 */
	using FileResolver = std::function<fs::File(const fs::File& disk_file)>;

	/**
	 * @brief The resolver that simply reads what is on disk.
	 */
	const FileResolver& identityFileResolver();

	// Regexes to reject files/directories starting with '.' or '$'
	const std::regex DEFAULT_REJECT_FILE_REGEX      = std::regex(R"((\$.*|\..*))");
	const std::regex DEFAULT_REJECT_DIRECTORY_REGEX = std::regex(R"((\$.*|\..*))");

	class ModuleTreeBuilder;
	class ModuleTreeModifier;
	struct GetModuleID_Functor;

	/**
	 * @brief Enumerates the different kinds of module tree nodes.
	 */
	enum class ModuleKind { Invalid, Module, Script, ReplChain };

	/**
	 * @brief Represents a single module tree node in the Duckling project tree.
	 *
	 * @note Each module is a standard module, a script module (created from a script file), or a
	 * synthetic REPL chain module (one statement of a REPL session or script).
	 *
	 * @note This is an abstract class, the instances include:
	 * - ModuleModuleTree: Represents a standard module.
	 * - script related nodes
	 *
	 * ModuleTree provides a hierarchical, in-memory representation of a module,
	 * including its main source file, submodules, and other files.
	 * The ModuleTree is the first instance of module in duckling compiling process
	 * the main use case is to build a module tree form existing folder, and then
	 * extract the pst from source files
	 * But module tree can be also created manually.
	 *
	 * - Tracks main source file, submodules, and other files.
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
	class ModuleTree {
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend struct GetModuleID_Functor;
		friend struct ModuleID;

	public:
		virtual ~ModuleTree();

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
		 *
		 * @note Only valid for modules that can have submodules (i.e., not REPL or script modules).
		 * @note Use this only if you need all submodules. For single submodule access, use
		 * getSubmoduleByName().
		 * @return A lazy view that can be unlocked within a query context or accessed illegally
		 * (outside queries).
		 */
		[[nodiscard]]
		virtual SubmodulesAccessLocked getSubmodules() const;

		/**
		 * Access a single submodule edge by name.
		 *
		 * @note Only valid for modules that can have submodules (i.e., not REPL or script modules).
		 *
		 * Registers dependency via QueryModuleChildSideInput when unlocked.
		 * Use this function in lookups when you need only a submodule with some name.
		 * @param name Name of the submodule to access.
		 * @return AccessLocked wrapper that may contain the submodule if it exists.
		 */
		[[nodiscard]]
		virtual ModuleChildAccessLocked getSubmoduleByName(base::StrID name) const;

		/**
		 * Accesses all the other files that are located inside the module.
		 * @return A base::HashMap that maps a file extension to a vector
		 * with files with this extension.
		 */
		[[nodiscard]]
		virtual const base::HashMap<base::StrID, std::vector<fs::File>>& getOtherFiles() const;

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

		[[nodiscard]]
		ModuleKind getKind() const;

		/**
		 * Check if this module is a REPL-generated module.
		 * REPL modules have special cross-module lookup behavior.
		 * @TODO: #2762 remove this if possible
		 * @return true if this is a REPL module, false otherwise
		 */
		[[nodiscard]]
		bool isReplModule() const {
			return getKind() == ModuleKind::ReplChain;
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

	protected:
		ModuleTree();

		/**
		 * Dummy function to make this class abstract.
		 */
		virtual void makeAbstract() = 0;

		/**
		 * @brief Access the slot holding the source file that acts as the main source file of this
		 * module.
		 *
		 * For a standard module this is ModuleModuleTreeNode::m_main_source_file, for a synthetic
		 * REPL chain module it is SyntheticReplChainModuleTreeNode::m_synthetic_source_file.
		 * @note Panics for module types that have no main source file (scripts). Check the module
		 * kind before calling this.
		 */
		[[nodiscard]]
		virtual Ref<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot();

		[[nodiscard]]
		virtual CRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() const;

		/**
		 * @brief Access the submodules of this module.
		 * @note Panics for module types that cannot have submodules. Check the module kind before
		 * calling this.
		 */
		[[nodiscard]]
		virtual Ref<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot();

		[[nodiscard]]
		virtual CRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() const;

		/**
		 * @brief Collects all child modules of this module.
		 * @return A vector containing references to all child modules.
		 *
		 * @important Submodules and child modules are not the same thing.
		 * Submodules reflect the high-level Duckling module structure,
		 * whereas child modules include all modules that are direct children in the module tree
		 * structure (i.e. whose parent is this module).
		 */
		[[nodiscard]]
		virtual std::vector<base::Ref<ModuleTree>> collectChildrenModules() const;

		/**
		 * @brief Access the other files of this module.
		 * @note Panics for module types that cannot have other files. Check the module kind before
		 * calling this.
		 */
		[[nodiscard]]
		virtual Ref<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot();

		[[nodiscard]]
		virtual CRef<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() const;

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
		 * @brief Add ModuleTree to static storage.
		 * @note In principle it should only be used in ModuleTreeBuilder::finalize.
		 * @return Reference to the stored ModuleTree, with its storage handle set.
		 */
		static base::Ref<ModuleTree> addModuleToStorage(base::Box<ModuleTree> module);

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


		/* * * * * * * * * * * * * * *\
		|  Universal data members:   *|
		\* * * * * * * * * * * * * * */

		ModuleKind kind = ModuleKind::Invalid;

		/**
		 * this is a self pointer, it is necessary to get the ModuleID from the const ModuleTree
		 */
		base::Optional<ModuleID> m_id;

		/**
		 * Package ID associated with this module tree.
		 * Used for component hash calculation.
		 */
		base::StrID m_package_id;

		/**
		 * Name of the module.
		 */
		base::StrID m_name;

		/**
		 * Parent module, if any.
		 * @note empty if this is a root module.
		 */
		base::Optional<base::Ref<ModuleTree>> m_parent;


		/**
		 * Key to support removal from static storage
		 */
		base::Optional<usize> m_storage_handle;

		/**
		 * ComponentHash of the module's logical path: eg
		 * package_name/root/submodule1/sub2
		 */
		base::Optional<hashing::ComponentHash> m_path_component_hash;

		/**
		 * This is the actual hash for the Module used in SideInput
		 */
		base::Optional<hashing::ComponentHash::HashType> m_hash;


		/**
		 * @brief Synchronizes lazy module hash/path-hash recomputation for this module.
		 * @note Hold this lock while reading/writing m_path_component_hash or m_hash during lazy
		 * recomputation flow (updateModuleHashFromRootToThis/updateModuleHash).
		 */
		mutable base::Box<std::mutex> m_hash_recompute_mutex;
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
