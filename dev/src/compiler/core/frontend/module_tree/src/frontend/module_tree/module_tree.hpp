// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "access.hpp"
#include "module_id.hpp"
#include "source_file.hpp"

#include <base/collections/hashmap.hpp>
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

	class ModuleTreeBuilderCommon;
	class StandardModuleTreeBuilder;
	class ScriptModuleTreeBuilder;
	class SyntheticReplChainModuleTreeBuilder;
	class ModuleTreeModifierCommon;
	class StandardModuleTreeModifier;
	class ScriptModuleTreeModifier;
	class SyntheticReplChainModuleTreeModifier;
	struct GetModuleID_Functor;

	/**
	 * @brief Enumerates the different kinds of module tree nodes.
	 */
	enum class ModuleKind {
		/**
		 * Uninitialized state.
		 */
		Invalid,

		/**
		 * Standard module.
		 */
		Module,

		/**
		 * Script module.
		 */
		Script,

		/**
		 * Synthetic REPL chain module.
		 */
		ReplChain
	};

	/**
	 * @brief Represents a single module tree node in the Duckling project tree.
	 *
	 * @note Each module is a standard module, a script module (created from a script file), or a
	 * synthetic REPL chain module (one statement of a REPL session or script).
	 *
	 * @note This is an abstract class, the instances include:
	 * - StandardModuleTreeNode: Represents a standard module.
	 * - ScriptModuleTreeNode / SyntheticReplChainModuleTreeNode: script and REPL nodes.
	 *
	 * @note Parent structure (m_parent):
	 * - a standard module or a script module has a standard module as parent, or none (root);
	 *   it is one of the parent's submodules.
	 * - a synthetic REPL chain module has the previous chain module or the script module that
	 *   starts the chain as parent, or none (first statement of a REPL session); it is the
	 *   parent's REPL child.
	 * Each node is built with the builder of its kind (StandardModuleTreeBuilder,
	 * ScriptModuleTreeBuilder, SyntheticReplChainModuleTreeBuilder) and modified with the
	 * matching modifier.
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
		friend class ModuleTreeBuilderCommon;
		friend class StandardModuleTreeBuilder;
		friend class ScriptModuleTreeBuilder;
		friend class SyntheticReplChainModuleTreeBuilder;
		friend class ModuleTreeModifierCommon;
		friend class StandardModuleTreeModifier;
		friend class ScriptModuleTreeModifier;
		friend class SyntheticReplChainModuleTreeModifier;
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
		 * For a standard module this is StandardModuleTreeNode::m_main_source_file, for a synthetic
		 * REPL chain module it is SyntheticReplChainModuleTreeNode::m_synthetic_source_file.
		 * A script module never has one.
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
		 * @brief Looks up the submodule `name` of this module.
		 * Registers the same dependency as StandardModuleTreeNode::getSubmoduleByName().
		 * @return Empty if there is no such submodule, or if this module cannot have submodules
		 * (script and REPL chain modules).
		 */
		[[nodiscard]]
		base::Optional<ModuleID> findSubmodule(query::Context& ctx, base::StrID name) const;

		/**
		 * @brief The number of submodules of this module, 0 for a script or REPL chain module.
		 * @note This registers no query dependency. Use it outside queries only.
		 */
		[[nodiscard]]
		usize getSubmoduleCount() const;

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

		/**
		 * @brief Downcasts this module to the node class of its kind.
		 * @note Panics if the module is not a T. Check the module kind before calling this.
		 */
		template<class T>
		Ref<T> as() {
			auto* node = dynamic_cast<T*>(this);
			CORE_ASSERT(node != nullptr, "ModuleTree::as<T>() called on a module of another kind");
			return node;
		}

		/**
		 * @brief Downcasts this module to the node class of its kind.
		 * @note Panics if the module is not a T. Check the module kind before calling this.
		 */
		template<class T>
		CRef<T> as() const {
			const auto* node = dynamic_cast<const T*>(this);
			CORE_ASSERT(node != nullptr, "ModuleTree::as<T>() called on a module of another kind");
			return node;
		}

		/**
		 * @brief Downcasts this module to the node class T, if it is one.
		 * @return The node as a T, or an empty optional when the module is of another kind.
		 */
		template<class T>
		base::Optional<Ref<T>> tryAs() {
			if (auto* node = dynamic_cast<T*>(this)) return Ref<T>(node);
			return {};
		}

		/**
		 * @brief Downcasts this module to the node class T, if it is one.
		 * @return The node as a T, or an empty optional when the module is of another kind.
		 */
		template<class T>
		base::Optional<CRef<T>> tryAs() const {
			if (const auto* node = dynamic_cast<const T*>(this)) return CRef<T>(node);
			return {};
		}

		/**
		 * @brief Collects all child modules of this module.
		 * @return A vector containing references to all child modules.
		 *
		 * @important Submodules and child modules are not the same thing.
		 * Submodules reflect the high-level Duckling module structure,
		 * whereas child modules include all modules that are direct children in the module tree
		 * structure (i.e. whose parent is this module): the submodules of a standard module, and
		 * the REPL child of a script or a REPL chain module.
		 *
		 * @TODO: #4253 This registers no query dependency, but probably should. 
		 * It is used inside module tree creation, so we want to be able to call it from inside a query (currenlty only script/repl pipeline does it, but we can refactor them so they don't), so we can generate module treies lazily as needed.
		 * There are two ways to go about it:
		 * 1. Refactor the repl/script pipeline and assume this will only be called outside queries.
		 * 2. Extend related sideinputs and modify this function to register query dependencies properly, allowing it to be called from within queries.
		 */
		[[nodiscard]]
		virtual std::vector<base::Ref<ModuleTree>> collectChildrenModules() const
			= 0;

		ModuleTree(const ModuleTree&)                = delete;
		ModuleTree& operator=(const ModuleTree&)     = delete;
		ModuleTree(ModuleTree&&) noexcept            = delete;
		ModuleTree& operator=(ModuleTree&&) noexcept = delete;

	protected:
		ModuleTree();

		/**
		 * @brief Collects all source files owned by this module.
		 * @return A vector containing references to the owned source files.
		 *
		 * @note as of now, its always 0 or 1 owned source file per module,
		 * but the interface returns a vector for ease of use and future extensibility.
		 */
		virtual std::vector<base::Ref<SourceFile>> collectOwnedSourceFiles() const = 0;

		/**
		 * @brief Adds the data of this module kind to the module hash.
		 *
		 * The module hash is computed from the root down (a parent's hash is always ready
		 * before its children's) and invalidateHash() only goes down to the children. So only
		 * data of this module or of its ancestors may be hashed here, never data of a child.
		 * Data that can change after finalize() must call invalidateHash() when it changes.
		 * Source file contents and submodules must not be hashed, they have their own side
		 * inputs.
		 */
		virtual void addKindSpecificDataToHash(hashing::ComponentHash::HashAlg& partial) const = 0;

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

		/**
		 * @brief Returns the module registered under the given module hash.
		 * A module is registered every time its hash is computed; entries are never removed, so a
		 * hash of a removed module that was not rebuilt resolves to a dangling reference.
		 */
		static base::Ref<ModuleTree> getRegisteredModule(const base::Bit256& hash);

		/* * * * * * * * * * * * * * *\
		|  Universal data members:   *|
		\* * * * * * * * * * * * * * */

		ModuleKind kind = ModuleKind::Invalid;

		/**
		 * this is a self pointer, it is necessary to get the ModuleID from the const ModuleTree
		 */
		base::Optional<base::Ref<ModuleTree>> m_self;

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
		 * @note empty if this is a root module. See the class doc for which kinds can be parents.
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
