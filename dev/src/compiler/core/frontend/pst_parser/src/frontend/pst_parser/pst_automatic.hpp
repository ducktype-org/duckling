/**
 * @file pst_automatic.hpp
 * @brief Useful parsing abstractions for LangParserState
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
 */
#pragma once

#include "access.hpp"
#include "lang_parser_element.hpp"

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/parser_state.hpp>

namespace pst {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	/**
	 * @brief Forces pass by value. Sometimes usefull in parse templates
	 */
	template<typename T>
	T fwdVal(T& t) {
		return t;
	}

	template<typename State>
	class PSTAutomatic {
	protected:
		State&                state;
		Ref<pst::LangElement> el;

	public:
		PSTAutomatic(State& state, Ref<pst::LangElement> caller): state(state), el(caller) {}

		PSTAutomatic(const PSTAutomatic&) = delete;

		// Useful for debugging:
		//
		// parses one of the available types
		// template<class T>
		// void one([[maybe_unused]]T t, [[maybe_unused]]bool ignorable = false) {
		// 	static_assert(sizeof(T) < 0, "parseOne for type `T` is not implemented\n");
		// }

		/**
		 * @brief Parses the expected keyword. Skips on success, logs error on failure.
		 * @param key The expected keyword.
		 */
		PSTAutomatic& one(Keyword key, bool ignorable = false) {
			if (!state.tryEat(key)) {
				state.log(makeBox<tpc::BadKeywordError>(state.getPosition(), key));
				if (!ignorable) skipNotSemicolon();
			} else {
				el->addToken(state[-1]);
			}
			return *this;
		}

		/**
		 * @brief Parses the expected Special token. Skips on success, logs error on failure.
		 * @param spec The expected special token.
		 */
		PSTAutomatic& one(Special spec, bool ignorable = false) {
			if (!state.tryEat(spec)) {
				state.log(makeBox<tpc::BadSpecialError>(state.getPosition(), spec));
				if (!ignorable) skipNotSemicolon();
			} else {
				el->addToken(state[-1]);
			}
			return *this;
		}

		/**
		 * @brief Parses the expected operator. Skips on success, logs error on failure.
		 * @param op The expected operator.
		 */
		PSTAutomatic& one(Operator op, bool ignorable = false) {
			if (!state.tryEat(op)) {
				state.log(makeBox<tpc::BadOperatorError>(state.getPosition(), op));
				if (!ignorable) skipNotSemicolon();
			} else {
				el->addToken(state[-1]);
			}
			return *this;
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
		 * @param result The place to store the parsed identifier.
		 */
		PSTAutomatic& one(tpc::Keyword* result, bool ignorable = false) {
			if (!state.ctokens().peek().isKeyword()) {
				state.log(makeBox<tpc::NoIdentifierError>(state.getPosition()));
				*result = Keyword::NotAKeyword;
				if (!ignorable) skipNotSemicolon();
				return *this;
			}
			el->addToken(state[0]);
			*result = lang_def::strAsKeyword(state.tokens().next().getValue());
			return *this;
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
		 * @param result The place to store the parsed identifier.
		 */
		PSTAutomatic& one(tpc::Identifier* result, bool ignorable = false) {
			if (!state.ctokens().peek().isIdentifier()) {
				state.log(makeBox<tpc::NoIdentifierError>(state.getPosition()));
				result->value = base::StrID("<error>");
				if (!ignorable) skipNotSemicolon();
				return *this;
			}
			el->addToken(state[0]);
			result->position = state.getPosition();
			result->value = state.tokens().next().getValue();
			return *this;
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
		 * @param result The place to store the parsed identifier.
		 */
		PSTAutomatic& one(tpc::OptionalIdentifier* result, [[maybe_unused]] bool ignorable = false) {
			if (state.ctokens().peek().isIdentifier()) {
				el->addToken(state[0]);
				result->position = state.getPosition();
				result->value = state.tokens().next().getValue();
			}
			return *this;
		}

		/**
		 * @brief Parses a string to @p result. Skips on success, does nothing on failure.
		 * @param result The place to store the parsed string.
		 */
		PSTAutomatic& one(tpc::StringValue* result, [[maybe_unused]] bool ignorable = false) {
			if (!state.ctokens().peek().isString()) {
				state.log(makeBox<tpc::NoStringError>(state.getPosition()));
				result->value = base::StrID("<error>");
				if (!ignorable) skipNotSemicolon();
				return *this;
			}
			el->addToken(state[0]);
			result->value = state.tokens().next().getValue();
			return *this;
		}

		/**
		 * @brief Parses a numeric value (with an optional type specifier) to @p result. Skips on
		 * success, does nothing on failure.
		 * @param result The place to store the parsed string.
		 */
		PSTAutomatic& one(tpc::NumericValue* result, [[maybe_unused]] bool ignorable = false) {
			if (!state.ctokens().peek().is(lexer::Token::Type::NumLiteralGroup)) {
				state.log(makeBox<tpc::NoNumericValueError>(state.getPosition()));
				result->value = base::StrID("<error>");
				result->type_specifier.reset();
				if (!ignorable) skipNotSemicolon();
				return *this;
			}

			el->addToken(state[0]);
			const auto& token      = state.tokens().next();
			const auto& sub_tokens = token.getRecursive();

			result->value = sub_tokens[0].getValue();
			if (sub_tokens.size() > 1)
				result->type_specifier = sub_tokens[1].getValue();
			else
				result->type_specifier.reset();

			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T>
		PSTAutomatic& one(MBox<T>* result, [[maybe_unused]] bool ignorable = false) {
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T, base::TemplateStringLiteral name>
		PSTAutomatic& one(AccessInternal<T, name>* result, [[maybe_unused]] bool ignorable = false) {
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T>
		PSTAutomatic& one(
			AccessInternalAnonymous<T>* result, [[maybe_unused]] bool ignorable = false
		) {
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T>
		PSTAutomatic& one(base::Optional<MBox<T>>* result, [[maybe_unused]] bool ignorable = false) {
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T, base::TemplateStringLiteral name>
		PSTAutomatic& one(
			base::Optional<AccessInternal<T, name>>* result, [[maybe_unused]] bool ignorable = false
		) {
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<std::derived_from<LangElement> El, base::TemplateStringLiteral name>
		PSTAutomatic& assign(AccessInternal<El, name>* sink, MBox<El>&& sub_tree) {
			if (sub_tree) {
				sub_tree->setParent(el);
				std::string str_name(name.value);
				el->addNamedChild(str_name, sub_tree.refMut());
				*sink = std::move(sub_tree);
			}
			return *this;
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<std::derived_from<LangElement> El, base::TemplateStringLiteral name>
		PSTAutomatic& assign(base::Optional<AccessInternal<El, name>>* sink, MBox<El>&& sub_tree) {
			if (sub_tree) {
				sub_tree->setParent(el);
				std::string str_name(name.value);
				el->addNamedChild(str_name, sub_tree.refMut());
				sink->emplace(std::move(sub_tree));
			}
			return *this;
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<std::derived_from<LangElement> El>
		PSTAutomatic& assign(AccessInternalAnonymous<El>* sink, MBox<El>&& sub_tree) {
			if (sub_tree) {
				sub_tree->setParent(el);
				el->addChild(sub_tree);
				*sink = std::move(sub_tree);
			}
			return *this;
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<std::derived_from<LangElement> El>
		PSTAutomatic& assign(base::Optional<AccessInternalAnonymous<El>>* sink, MBox<El>&& sub_tree) {
			if (sub_tree) {
				sub_tree->setParent(el);
				el->addChild(sub_tree);
				sink->emplace(std::move(sub_tree));
			}
			return *this;
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<std::derived_from<LangElement> El, typename Sink>
		PSTAutomatic& assign(Sink* sink, MBox<El>&& sub_tree) {
			if (sub_tree) *sink = std::move(sub_tree);
			return *this;
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
		template<std::derived_from<LangElement> El, typename Sink, typename... Args>
		PSTAutomatic& with(Sink* sink, MBox<El> fun(State&, Args...), Args&&... args) {
			MBox<El> result = fun(state, std::forward<Args>(args)...);
			assign(sink, std::move(result));
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 *
		 * The return type of the parsed function usually has to be specified with the first
		 * template argument.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param args Arguments passed to the parsing function
		 */
		template<std::derived_from<LangElement> El, typename Sink, typename... Args>
		PSTAutomatic& withDef(Sink* sink, Args&&... args) {
			MBox<El> result = El::parse(state, std::forward<Args>(args)...);
			assign(sink, std::move(result));
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(MBox<El>* result, Args&&... args) {
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, base::TemplateStringLiteral name, typename... Args>
		PSTAutomatic& withDef(AccessInternal<El, name>* result, Args&&... args) {
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(AccessInternalAnonymous<El>* result, Args&&... args) {
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(base::Optional<MBox<El>>* result, Args&&... args) {
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, base::TemplateStringLiteral name, typename... Args>
		PSTAutomatic& withDef(base::Optional<AccessInternal<El, name>>* result, Args&&... args) {
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(base::Optional<AccessInternalAnonymous<El>>* result, Args&&... args) {
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Automatic version of the ParserState function
		 *
		 * @note this allows to eat a semicolon as it has to be specified
		 */
		template<typename T>
		bool tryEat(T type) {
			if (state.notEmpty() && state[0].is(type)) {
				el->addToken(state.tokens().next());
				return true;
			}
			return false;
		}

		/**
		 * @brief Eats any token other then a semicolon
		 */
		PSTAutomatic& eatOne() {
			if (state.notEmpty() && !state[0].is(Special::Semicolon))
				el->addToken(state.tokens().next());
			return *this;
		}

		/**
		 * @brief Skips any token other then a semicolon
		 */
		PSTAutomatic& skipNotSemicolon() {
			if (state.notEmpty() && !state[0].is(Special::Semicolon)) state.tokens().skip();
			return *this;
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		PSTAutomatic& goDown() {
			el->addToken(state[0].getSentinelBegin());
			state.goDown();
			return *this;
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		PSTAutomatic& goUpAndSkip() {
			state.goUp();
			el->addToken(state[0].getSentinelEnd());
			skipNotSemicolon();
			return *this;
		}

		/**
		 * @brief Parses all of the given elements.
		 * @note Forces the first element to be skipped on error if it's a token.
		 */
		template<typename T>
		PSTAutomatic& all(T t) {
			one(t);
			return *this;
		}

		/**
		 * @brief Parses all of the given elements.
		 * @note Forces the first element to be skipped on error if it's a token.
		 */
		template<typename T, typename... Q>
		PSTAutomatic& all(T t, Q... q) {
			one(t);
			parseRest(q...);
			return *this;
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
}
