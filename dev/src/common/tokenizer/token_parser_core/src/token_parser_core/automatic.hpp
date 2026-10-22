// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file automatic.hpp
 * @brief Useful parsing abstractions for ParserState
 *
 * one() - has four modes depending on the type of the first argument:
 *  - for Specials, Keywords and Operators from `lang_def` it ensures that the next token has that
 * value and skips it, otherwise it logs an error
 *  - for Identifier* it ensures the next token is an identifier and parses it to the specified
 * location and skips it, otherwise it logs an error
 *  - for Box<T>* it calls the parser of T object into the specified location
 *
 * The optional argument ignorable additionally allows to control behaviour in case of error.
 * If it's set to true then simple parse-able entities(not parser ref) will not be skipped on error.
 * It works as a kind of assumption that is something simple doesn't fit then it's missing not
 * wrong.
 *
 * all() takes the state and any number of additional arguments and calls parseOne on those
 * arguments from left to right. Additionally it makes the first parsed thing non-ignorable and the
 * rest ignorable so that infinite parsing loops are very unlikely.
 *
 * NullAwareDprint is a wrapper for element specific debug prints called on pointers that prints
 * null if the pointer is null
 */
#pragma once

#include "base_element.hpp"
#include "common_elements.hpp"

#include <diagnostic/message.hpp>
#include <diagnostic/source_position.hpp>
#include <lang_definitions/key_spec_op.hpp>

#include <concepts>

namespace tpc {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	class BadKeywordError;
	class BadSpecialError;
	class BadOperatorError;
	class NoIdentifierError;

	template<typename State>
	class GenericAutomatic {
	protected:
		State& state;

	public:
		GenericAutomatic(State& state): state(state) {}

		GenericAutomatic(const GenericAutomatic&) = delete;

		// Useful for debugging:
		//
		// parses one of the available types
		// template<class T>
		// void one([[maybe_unused]]T t, [[maybe_unused]]bool) {
		// 	static_assert(sizeof(T) < 0, "parseOne for type `T` is not implemented\n");
		// }

		/**
		 * @brief Parses the expected keyword. Skips on success, logs error on failure.
		 * @param key The expected keyword.
		 */
		void one(Keyword key, bool ignorable = false) {
			if (!state.tryEat(key)) {
				state.logInt(makeBox<BadKeywordError>(state.getPosition(), key));
				if (!ignorable) state.tokens().next();
			}
		}

		/**
		 * @brief Parses the expected Special token. Skips on success, logs error on failure.
		 * @param spec The expected special token.
		 */
		void one(Special spec, bool ignorable = false) {
			if (!state.tryEat(spec)) {
				state.logInt(makeBox<BadSpecialError>(state.getPosition(), spec));
				if (!ignorable) state.tokens().next();
			}
		}

		/**
		 * @brief Parses the expected operator. Skips on success, logs error on failure.
		 * @param op The expected operator.
		 */
		void one(Operator op, bool ignorable = false) {
			if (!state.tryEat(op)) {
				state.logInt(makeBox<BadOperatorError>(state.getPosition(), op));
				if (!ignorable) state.tokens().next();
			}
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
		 * @param result The place to store the parsed identifier.
		 */
		void one(Identifier* result, bool ignorable = false) {
			result->position = state.getPosition();
			if (!state.ctokens().peek().isIdentifier()) {
				state.logInt(makeBox<NoIdentifierError>(
					state.getPosition(), state.ctokens().peek().describe()
				));
				result->value = base::StrID("<error>");
				if (!ignorable) state.tokens().next();
				return;
			}
			result->value = state.tokens().next().getValue();
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<Element> T>
		void one(Box<T>* result, bool = false) {
			*result = T::parse(state);
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<Element> T>
		void one(MBox<T>* result, bool = false) {
			*result = T::parse(state);
		}

		/**
		 * @brief Parses all of the given elements.
		 * @note Forces the first element to be skipped on error if it's a token.
		 */
		template<typename T>
		void all(T t) {
			one(t);
		}

		/**
		 * @brief Parses all of the given elements.
		 * @note Forces the first element to be skipped on error if it's a token.
		 */
		template<typename T, typename... Q>
		void all(T t, Q... q) {
			one(t);
			parseRest(q...);
		}

		/**
		 * @brief Call a custom parse function with automation.
		 *
		 * The return type of the parsed function usually has to be specified with the first
		 * template argument.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun Parsing function.
		 * @param args Arguments passed to the parsing function
		 */
		template<std::derived_from<Element> El, typename Sink, typename... Args>
		void with(Sink* sink, Box<El> fun(State&, Args...), Args&&... args) {
			*sink = fun(state, std::forward<Args>(args)...);
		}

	private:
		/**
		 * @brief Parses all of the given elements.
		 */
		template<typename T>
		void parseRest(T t) {
			one(t, true);
		}

		/**
		 * @brief Parses all of the given elements.
		 */
		template<typename T, typename... Q>
		void parseRest(T t, Q... q) {
			one(t, true);
			parseRest(q...);
		}
	};

	class BadKeywordError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_keyword" };
		}

	public:
		BadKeywordError(dia::SourcePosition pos, Keyword key):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("keyword", lang_def::keywordToStr(key).str());
		}
	};

	class BadSpecialError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_special" };
		}

	public:
		BadSpecialError(dia::SourcePosition pos, Special spec):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("special", lang_def::specialToStr(spec).str());
		}
	};

	class BadOperatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_operator" };
		}

	public:
		BadOperatorError(dia::SourcePosition pos, Operator opr):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("operator", opr.str());
		}
	};

	class NoIdentifierError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "parser",
				.name          = "no_identifier",
			};
		}

	public:
		NoIdentifierError(dia::SourcePosition pos, std::string_view but_got);
	};

	class NoKeywordError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "parser",
				.name          = "no_keyword",
			};
		}

	public:
		NoKeywordError(dia::SourcePosition pos, std::string_view but_got);
	};

	class NoOperatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "parser",
				.name          = "no_operator",
			};
		}

	public:
		NoOperatorError(dia::SourcePosition pos, std::string_view but_got);
	};

	class NoStringError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_string" };
		}

	public:
		NoStringError(dia::SourcePosition pos): MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoNumericValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_numeric_value" };
		}

	public:
		NoNumericValueError(dia::SourcePosition pos): MessageWithCodeFragmentAndCause(pos) {}
	};

}
