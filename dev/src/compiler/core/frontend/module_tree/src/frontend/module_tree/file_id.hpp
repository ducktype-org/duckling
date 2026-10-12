// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/bit256.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::frontend {

	class SourceFile;
	struct ModuleID;

	/**
	 * @brief FileID is a unique identifier for a SourceFile in the Duckling compiler.
	 *
	 * FileID wraps a reference to a SourceFile and provides hashing.
	 * It is used to track and query source files in module trees.
	 * FileID is not constructible from outside; use functors or friend classes to create
	 * instances.
	 */
	struct FileID final {
		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hash;
		}

		bool operator==(const FileID&) const = default;

		auto operator<=>(const FileID&) const = default;

	private:
		/**
		 * @brief Creates the ID from the current component hash of the file and registers the file
		 * under that hash.
		 */
		FileID(base::Ref<SourceFile> ref);

		/**
		 * @brief Resolves the ID to the file currently registered under its hash.
		 * In development builds it panics when that file was removed from storage.
		 */
		[[nodiscard]] base::Ref<SourceFile> resolve() const;

		base::Bit256 hash;

		friend class SourceFile;
		friend struct GetFileID_Functor;
	};
}
