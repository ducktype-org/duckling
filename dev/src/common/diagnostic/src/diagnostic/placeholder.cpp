// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "placeholder.hpp"

#include <diagnostic/stable_position.hpp>
#include <logger/logger.hpp>

namespace dia {

#define ADD_ARGUMENTS()                                                       \
	addArgument<TextArgument>("header_message", std::move(header_message));   \
	addArgument<TextArgument>("description", std::move(description));         \
	if_opt_some(source_position, pos) {                                       \
		addArgument<CodeArgument>("code", pos);                               \
		addArgument<CodeLocationArgument>("code_location", pos);              \
		addPointerMessage("cause", pos);                                      \
	}                                                                         \
	if_opt_some(pointer_message_content, val) {                               \
		addArgument<TextArgument>("pointer_message_content", std::move(val)); \
	}                                                                         \
	if_opt_none(pointer_message_content) {                                    \
		addArgument<TextArgument>("pointer_message_content", "");             \
	}

	PlaceholderError::PlaceholderError(
		std::string                         header_message,
		base::Optional<dia::SourcePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
	}

	PlaceholderError::PlaceholderError(
		std::string                         header_message,
		base::Optional<dia::StablePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
	}

	PlaceholderError::PlaceholderError(std::string header_message, std::string description):
		  PlaceholderError(
			  std::move(header_message),
			  base::Optional<dia::SourcePosition>{},
			  std::move(description),
			  {}
		  ) {}

	PlaceholderNote::PlaceholderNote(
		std::string                         header_message,
		base::Optional<dia::SourcePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
	}

	PlaceholderNote::PlaceholderNote(
		std::string                         header_message,
		base::Optional<dia::StablePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
	}

	PlaceholderNote::PlaceholderNote(std::string header_message, std::string description):
		  PlaceholderNote(
			  std::move(header_message),
			  base::Optional<dia::SourcePosition>{},
			  std::move(description),
			  {}
		  ) {}

	NotYetImplementedCodeError::NotYetImplementedCodeError(
		std::string                         header_message,
		base::Optional<dia::SourcePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
		addStacktraceArgument();
	}

	NotYetImplementedCodeError::NotYetImplementedCodeError(
		std::string                         header_message,
		base::Optional<dia::StablePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
		addStacktraceArgument();
	}

	void NotYetImplementedCodeError::addStacktraceArgument() {
		if (logger::isCategoryEnabled(logger::DevLogCategories::NYIStacktraces)) {
			addArgument<TextArgument>("stacktrace", base::getCurrentStackTrace());
		} else {
			addArgument<TextArgument>(
				"stacktrace",
				"Enable the NYIStacktraces dev-logs category to see the stacktrace for this "
				"not-yet-implemented error."
			);
		}
	}

	NotYetImplementedCodeError::NotYetImplementedCodeError(
		std::string header_message, std::string description
	):
		  NotYetImplementedCodeError(
			  std::move(header_message),
			  base::Optional<dia::SourcePosition>{},
			  std::move(description),
			  {}
		  ) {}

	PlaceholderWarning::PlaceholderWarning(std::string header_message, std::string description):
		  PlaceholderWarning(
			  std::move(header_message),
			  base::Optional<dia::SourcePosition>{},
			  std::move(description),
			  {}
		  ) {}

	PlaceholderWarning::PlaceholderWarning(
		std::string                         header_message,
		base::Optional<dia::StablePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
	}

	PlaceholderWarning::PlaceholderWarning(
		std::string                         header_message,
		base::Optional<dia::SourcePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		ADD_ARGUMENTS();
	}
}
