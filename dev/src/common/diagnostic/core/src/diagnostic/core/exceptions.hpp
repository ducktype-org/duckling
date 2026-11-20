// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <exception>
#include <string>
#include <utility>

namespace dia {

	/**
	 * @brief Exception intended to be the basis of all non-panic duckling-specific exceptions.
	 */
	class Exception: public std::exception {};

	/**
	 * @brief Exception thrown when template evaluation fails.
	 */
	class TemplateEvaluationException final: public Exception {
		std::string message;

	public:
		explicit TemplateEvaluationException(std::string message): message(std::move(message)) {}

		[[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
	};

	/**
	 * @brief Exception thrown when parsing a template file fails.
	 */
	class ParsingTemplateFileError final: public Exception {
		std::string message;

	public:
		explicit ParsingTemplateFileError(std::string message): message(std::move(message)) {}

		[[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
	};

	/**
	 * @brief Exception thrown when parsing a diagnostic file fails.
	 */
	class ParsingDiagnosticFileError final: public Exception {
		std::string message;

	public:
		explicit ParsingDiagnosticFileError(std::string message): message(std::move(message)) {}

		[[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
	};

}
