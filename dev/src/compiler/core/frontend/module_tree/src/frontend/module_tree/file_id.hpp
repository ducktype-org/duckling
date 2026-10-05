// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>

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
		u64 queryUnstablePerfectHash() const {
			checkDanglingReference();
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const FileID&) const = default;

		auto operator<=>(const FileID& other) const { return ref.get() <=> other.ref.get(); }

	private:
		FileID(base::Ref<SourceFile> ref): ref(ref) {}

		/**
		 * @brief Ensures the referenced SourceFile is still valid during development builds.
		 * This function will work only if use_module_modifier_remove is enabled.
		 */
		void checkDanglingReference() const;

		base::Ref<SourceFile> ref;

		friend class SourceFile;
		friend struct GetFileID_Functor;
	};
}
