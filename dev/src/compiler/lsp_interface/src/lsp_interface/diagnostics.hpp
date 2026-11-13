// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>
#include <lsp_interface/files_cache.hpp>

#include <base/pointers/ref.hpp>

#include <diagnostic/core/diagnostic_arguments_forward.hpp>

#include <string>

namespace duck_ls {
	/**
	 * @brief The result of evaluating a diagnostic into its Language Server Protocol form.
	 *
	 * The protocol puts the file outside the diagnostic itself, because a
	 * `textDocument/publishDiagnostics` notification carries one URI and the diagnostics
	 * belonging to it. Ours come from a whole package at once, so each result says which file
	 * it belongs to and the caller groups them.
	 */
	struct LSPDiagnosticResult final {
		/// The diagnostic, ready to send.
		::lsp::Diagnostic diagnostic{};

		/// The file the diagnostic belongs to.
		::lsp::Uri uri{};
	};

	/**
	 * @brief Everything needed to evaluate a diagnostic into a language server message.
	 *
	 * Some diagnostics carry no location at all, so evaluation needs both a fallback file and
	 * the files cache that names a path with the URI to answer the client with.
	 */
	class EvaluationContext {
	public:
		/**
		 * @param default_error_location Path to attach diagnostics that carry no location to.
		 * @param query_file Path of the file diagnostics were requested for.
		 * @param files The cache naming paths with the URIs the client knows them by.
		 */
		EvaluationContext(
			fs::FilePath default_error_location, fs::FilePath query_file, base::Ref<FilesCache> files
		):
			  default_error_location(std::move(default_error_location)),
			  queried_file(std::move(query_file)),
			  files(files) {}

		/**
		 * @brief Resolves a path to the URI the client knows it by.
		 */
		[[nodiscard]] ::lsp::Uri resolve(const fs::FilePath& path) const {
			return files->uriFor(path);
		}

		/// The default file path to use for diagnostics without location.
		fs::FilePath default_error_location;

		/// The path of the file we are querying diagnostics for.
		fs::FilePath queried_file;

		/// Names paths with the URIs the client knows them by.
		base::Ref<FilesCache> files;
	};

	/**
	 * @brief Evaluate diagnostic arguments to a language server message.
	 */
	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia::dia_args::Diagnostic> diagnostic_args, const EvaluationContext& evaluation_ctx
	);
}
