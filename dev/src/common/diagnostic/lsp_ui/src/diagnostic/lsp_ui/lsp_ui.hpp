#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>

#include <diagnostic/core/diagnostic_arguments_forward.hpp>

#include <functional>
#include <string>

namespace dia::lsp {
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
		::lsp::Diagnostic diagnostic;

		/// The file the diagnostic belongs to.
		::lsp::Uri uri;
	};

	/**
	 * @brief Everything needed to evaluate a diagnostic into a language server message.
	 *
	 * Diagnostics carry a file as a bare path, and some carry no location at all, so evaluation
	 * needs both a fallback file and a way to turn a path into the URI to answer the client
	 * with. Building that URI needs percent encoding and, for a file the editor holds open, the
	 * spelling the client itself used, so the language server supplies it.
	 */
	class EvaluationContext {
	public:
		/// Turns the path a diagnostic carries into the URI to name it by.
		using UriResolver = std::function<::lsp::Uri(const std::string& path)>;

		/**
		 * @param default_error_location Path to attach diagnostics that carry no location to.
		 * @param query_file Path of the file diagnostics were requested for.
		 * @param to_uri Turns a path into the URI to answer the client with.
		 */
		EvaluationContext(
			std::string default_error_location, std::string query_file, UriResolver to_uri
		):
			  default_error_location(std::move(default_error_location)),
			  queried_file(std::move(query_file)),
			  to_uri(std::move(to_uri)) {}

		/**
		 * @brief Resolves a path to its URI, falling back to the default location when empty.
		 */
		[[nodiscard]] ::lsp::Uri resolve(const std::string& path) const {
			return to_uri(path.empty() ? default_error_location : path);
		}

		/// The default file path to use for diagnostics without location.
		std::string default_error_location;

		/// The path of the file we are querying diagnostics for.
		std::string queried_file;

		/// Turns a path into the URI to answer the client with.
		UriResolver to_uri;
	};

	/**
	 * @brief Evaluate diagnostic arguments to a language server message.
	 */
	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, const EvaluationContext& evaluation_ctx
	);
}
