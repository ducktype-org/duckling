#pragma once

#include <diagnostic_interactive/core/diagnostic_arguments_forward.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <string>

namespace dia_int::lsp {
	class Diagnostic;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia_int::lsp::Diagnostic);

namespace dia_int::lsp {
	class LSPDiagnosticResult {
	public:
		Box<Diagnostic> diagnostic;
		std::string     file_uri;

		LSPDiagnosticResult(Box<Diagnostic> diag, std::string file_uri = {}):
			  diagnostic(std::move(diag)),
			  file_uri(std::move(file_uri)) {}

		static void serializeDiagnostic(CRef<Diagnostic> diagnostic, std::ostream& out);
	};

	class EvaluationContext {
		std::string default_file_location;
	public:
		std::string (*convert_path_to_uri)(const std::string&);

		EvaluationContext(
			std::string default_file_location, std::string (*path_to_uri)(const std::string&)
		):
			  default_file_location(std::move(default_file_location)),
			  convert_path_to_uri(path_to_uri) {
			CORE_ASSERT(this->convert_path_to_uri != nullptr, "path_to_uri function pointer cannot be null");
		}

		[[nodiscard]] std::string defaultUri() const { return convert_path_to_uri(default_file_location); }
	};

	/**
	 * @brief Evaluate diagnostic to language server message and print it to the given stream.
	 */
	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, const EvaluationContext& ctx
	);
}
