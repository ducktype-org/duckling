/**
 * @file automatic.hpp
 * @brief Useful parsing abstractions for ParserState
 *
 * one() - has four modes depending on the type of the first argument:
 *  - for Specials, Keywords and Operators from `lang_def` it ensures that the next token has that
 * value and skips it, otherwise it logs an error
 *  - for Identifier* it ensures the next token is an identifier and parses it to the specified
 * location and skips it, otherwise it logs an error
 *  - for OptionalIdentifier* it parses an identifier into the specified location and skips. If
 * There is no identifier next it doesn't do anything
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

#include <concepts>

#include "base_element.hpp"

#include "common_elements.hpp"

#include "parser_state.hpp"
#include <diagnostic/source_position.hpp>
#include <lang_definitions/key_spec_op.hpp>

namespace tpc {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	void nullAwareDprint(Identifier, std::ostream& out);
	void nullAwareDprint(OptionalIdentifier, std::ostream& out);
	void nullAwareDprint(Keyword, std::ostream& out);
	void nullAwareDprint(Operator, std::ostream& out);
	void nullAwareDprint(Special, std::ostream& out);

	template<typename T>
	void nullAwareDprint(const Box<T>& ref, std::ostream& out) {
		// This templates's logic is very weird...
		// It implies that if not bool(*ref) then <nullptr> else dprint...
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->debugPrint(out);
	}

	template<typename T>
	void nullAwareDprint(const MBox<T>& ref, std::ostream& out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->debugPrint(out);
	}

	template<typename T>
	void nullAwareDprint(MCRef<T> ref, std::ostream& out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->debugPrint(out);
	}

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
				state.log(makeBox<BadKeywordError>(state.getPosition(), key));
				if (!ignorable) state.tokens().next();
			}
		}

		/**
		 * @brief Parses the expected Special token. Skips on success, logs error on failure.
		 * @param spec The expected special token.
		 */
		void one(Special spec, bool ignorable = false) {
			if (!state.tryEat(spec)) {
				state.log(makeBox<BadSpecialError>(state.getPosition(), spec));
				if (!ignorable) state.tokens().next();
			}
		}

		/**
		 * @brief Parses the expected operator. Skips on success, logs error on failure.
		 * @param op The expected operator.
		 */
		void one(Operator op, bool ignorable = false) {
			if (!state.tryEat(op)) {
				state.log(makeBox<BadOperatorError>(state.getPosition(), op));
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
				state.log(makeBox<NoIdentifierError>(state.getPosition()));
				result->value = base::StrID("<error>");
				if (!ignorable) state.tokens().next();
				return;
			}
			result->value = state.tokens().next().getValue();
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
		 * @param result The place to store the parsed identifier.
		 */
		void one(OptionalIdentifier* result, bool = false) {
			if (state.ctokens().peek().isIdentifier()) {
				result->position = state.getPosition();
				result->value    = state.tokens().next().getValue();
			}
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

	class BadKeywordError final: public dia::Error {
	private:
		Keyword expected;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected keyword `" + lang_def::keywordToStr(expected).str() + "` here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadKeywordError(dia::SourcePosition pos, Keyword key): dia::Error(pos), expected(key) {}
	};

	class BadSpecialError final: public dia::Error {
	private:
		Special expected;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected special `" + lang_def::specialToStr(expected).str() + "` here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadSpecialError(dia::SourcePosition pos, Special spec): dia::Error(pos), expected(spec) {}
	};

	class BadOperatorError final: public dia::Error {
	private:
		Operator expected;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected operator `" + expected.str() + "` here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadOperatorError(dia::SourcePosition pos, Operator opr): dia::Error(pos), expected(opr) {}
	};

	class NoIdentifierError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected an identifier here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NoIdentifierError(dia::SourcePosition pos): dia::Error(pos) {}
	};
}
