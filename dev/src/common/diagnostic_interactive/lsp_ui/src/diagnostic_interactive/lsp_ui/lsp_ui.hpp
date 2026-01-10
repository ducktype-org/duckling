#pragma once

#include <diagnostic_interactive/core/diagnostic_arguments_forward.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <string>

namespace dia_int::lsp {
	/**
	 * @brief The diagnostic class is 1-1 mapping of the typescript 
	 * Language Server Protocol `Diagnostic` structure.
	 */
	class Diagnostic;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia_int::lsp::Diagnostic);

namespace dia_int::lsp {
	/**
	 * @brief The result of the evaluation of a diagnostic to a language server protocol diagnostic.
	 *
	 * There was an issue with the protocol, that the content of the message doesn't have the file URI.
	 * In our setup we want to output diagnostics from different files and we should group them by file URI,
	 * so that is why this `file_uri` field is here separately. 
	 */
	class LSPDiagnosticResult {
	public:
		/**
		* @brief Langauge Server Protocol diagnostic.
		*/
		Box<Diagnostic> diagnostic;

		/**
		 * @brief File URI of the file the diagnostic belongs to.
		 */
		std::string     file_uri;

		LSPDiagnosticResult(Box<Diagnostic> diag, std::string file_uri = {}):
			  diagnostic(std::move(diag)),
			  file_uri(std::move(file_uri)) {}

		static void jsonSerializeDiagnostic(CRef<Diagnostic> diagnostic, std::ostream& out);
	};

	/**
	 * These is the context needed to evaluate diagnostics to language server messages.
	 *
	 * Some errors may not have location information, so we need a context to provide
	 * default location information and also the mapping from the VFS paths to URIs.
	 */
	class EvaluationContext {
		/**
		 * @brief The default file location to use for diagnostics without location.
		 */
		std::string default_file_location;

	public:
		/**
		 * @brief Function pointer to convert a (typically virtual fs) file path to a URI.
		 */
		std::string (*convert_path_to_uri)(const std::string&);

		EvaluationContext(
			std::string default_file_location, std::string (*path_to_uri)(const std::string&)
		):
			  default_file_location(std::move(default_file_location)),
			  convert_path_to_uri(path_to_uri) {
			CORE_ASSERT(
				this->convert_path_to_uri != nullptr, "path_to_uri function pointer cannot be null"
			);
		}

		[[nodiscard]] std::string defaultUri() const {
			return convert_path_to_uri(default_file_location);
		}
	};

	/**
	 * @brief Evaluate diagnostic arguments to language server message.
	 */
	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, const EvaluationContext& ctx
	);
}
