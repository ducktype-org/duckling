// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/source_file.hpp>

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <query_framework/external/api.hpp>

#include <span>
#include <vector>

/**
 * @file file_pst_inputs.hpp
 * @brief PST inputs of source files remembered between compilations.
 *
 * At the start of an incremental compilation the driver needs the PST access side inputs (unlock
 * group hashes) of all source files to mark the previous query graph. Computing them requires
 * parsing every file, which dominates incremental rebuilds. Instead, every compilation stores, for
 * every source file, the SHA256 of its content and its PST inputs; a file whose content hash
 * matches the stored one contributes the stored inputs without being parsed (its PST is parsed
 * lazily, only if a query needs it). PST hashes depend only on the file content and its component
 * path (the record key), and artifacts are discarded when the compiler build changes, so the
 * stored inputs are exactly the ones the parse would produce.
 *
 * For a single file the rule is always the same:
 * 1. if the file has a PST, its inputs are computed from the PST,
 * 2. else, if the previous record of the file has the same content hash, the recorded inputs,
 * 3. else the file is parsed.
 *
 * \parallel The previous records are loaded before any query runs and are read-only afterwards.
 * SourceFile parsing and content hashing are synchronized by the SourceFile itself.
 */
namespace compiler::driver {

	/**
	 * @brief Loads the records stored by the previous compilation. Malformed data is ignored (all
	 * files are then parsed). Must be called before any query runs.
	 */
	void loadPreviousFilePSTInputs(std::span<const byte> data);

	/**
	 * @brief Makes sure the PST inputs of all source files of the global packages can be
	 * collected: files without a matching previous record are parsed (in parallel).
	 * @note Cannot be called from a query.
	 */
	void prepareFilesForInputCollection();

	/**
	 * @brief Appends the PST access side inputs of the source file (rule above).
	 */
	void collectFilePSTInputs(
		Ref<frontend::SourceFile> source_file, std::vector<query::external::InputData>& out
	);

	/**
	 * @brief Serializes the records of all source files of the global packages that are either
	 * parsed or have a matching previous record.
	 * @note Should be called at the end of the compilation, when no queries are running.
	 */
	[[nodiscard]] std::vector<byte> serializeFilePSTInputs();

	/**
	 * @brief Parses all not yet parsed source files of the global packages (sequentially, so it
	 * can be called from a query). Used when a PST element is looked up by a stable hash that is
	 * not known, e.g. a hash from a result loaded from disk that points into a file that was not
	 * parsed in this compilation.
	 */
	void parseAllUnparsedFiles();
}
