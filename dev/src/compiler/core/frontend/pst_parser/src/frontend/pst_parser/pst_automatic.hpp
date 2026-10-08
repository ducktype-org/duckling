// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file pst_automatic.hpp
 * @brief Useful parsing abstractions for LangParserState
 *
 * one() - has four modes depending on the type of the first argument:
 *  - for Specials, Keywords and Operators from `lang_def` it ensures that the next token has that
 * value and skips it, otherwise it logs an error
 *  - for Identifier* it ensures the next token is an identifier and parses it to the specified
 * location and skips it, otherwise it logs an error
 *  - for Optional<AccessInternal<IdentifierWrapper>>* it parses an identifier into the specified
 * location and skips. If There is no identifier next it doesn't do anything
 *  - for Box<T>* it calls the parser of T object into the specified location
 *  - for AccessInternal(Anonymous)<T>* it calls the parser of T object into the specified location
 * while doing additional work to connect it with the parent element.
 *
 * all() takes the state and any number of additional arguments and calls parseOne on those
 * arguments from left to right.
 */
#pragma once

#include "access.hpp"
#include "lang_parser_element.hpp"

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/parser_state.hpp>

#define PST_AUTOMATIC_SKIP(ret) \
	if (state.isSkipping()) { return ret; }

#define PARSE() state.parse(out)
/**
 * @brief This macro saves the current context and restores it after executing code from the argument.
 */
#define PST_NEW_CONTEXT(code)       \
	state.parse(out).saveContext(); \
	code;                           \
	state.parse(out).exitSoftFallback();

namespace pst {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	class LangParserState;
	using TokenStreamCondition = bool(const TokenStream&, i64);

	/**
	 * @brief Place for automatic sub-functionalities that aren't dependent on the state
	 */
	class FreeAutomatic {
	public:
		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param parent The parent element.
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<
			std::derived_from<LangElement> ParentEl,
			std::derived_from<LangElement> El,
			base::TemplateStringLiteral    name,
			std::derived_from<LangElement> El2>
		static void assign(
			Ref<ParentEl> parent, AccessInternal<El, name>* sink, MBox<El2>&& sub_tree
		) {
			if (sub_tree) {
				sub_tree->setParent(parent);
				std::string str_name(name.value);
				parent->addNamedChild(str_name, sub_tree.refMut());
				*sink = std::move(sub_tree);
			}
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param parent The parent element.
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<
			std::derived_from<LangElement> ParentEl,
			std::derived_from<LangElement> El,
			base::TemplateStringLiteral    name,
			std::derived_from<LangElement> El2>
		static void assign(
			Ref<ParentEl>                             parent,
			base::Optional<AccessInternal<El, name>>* sink,
			MBox<El2>&&                               sub_tree
		) {
			if (sub_tree) {
				sub_tree->setParent(parent);
				std::string str_name(name.value);
				parent->addNamedChild(str_name, sub_tree.refMut());
				sink->emplace(std::move(sub_tree));
			}
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param parent The parent element.
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<
			std::derived_from<LangElement> ParentEl,
			std::derived_from<LangElement> El,
			std::derived_from<LangElement> El2>
		static void assign(
			Ref<ParentEl> parent, AccessInternalAnonymous<El>* sink, MBox<El2>&& sub_tree
		) {
			if (sub_tree) {
				sub_tree->setParent(parent);
				parent->addChild(sub_tree);
				*sink = std::move(sub_tree);
			}
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param parent The parent element.
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<
			std::derived_from<LangElement> ParentEl,
			std::derived_from<LangElement> El,
			std::derived_from<LangElement> El2>
		static void assign(
			Ref<ParentEl>                                parent,
			base::Optional<AccessInternalAnonymous<El>>* sink,
			MBox<El2>&&                                  sub_tree
		) {
			if (sub_tree) {
				sub_tree->setParent(parent);
				parent->addChild(sub_tree);
				sink->emplace(std::move(sub_tree));
			}
		}

		/**
		 * @brief Assigned an already parsed subtree to a variable with all of the automation.
		 *
		 * @param parent The parent element.
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun The value.
		 */
		template<std::derived_from<LangElement> ParentEl, std::derived_from<LangElement> El, typename Sink>
		static void assign(Ref<ParentEl>, Sink* sink, MBox<El>&& sub_tree) {
			if (sub_tree) *sink = std::move(sub_tree);
		}
	};

	template<typename State>
	class PSTAutomatic {
	protected:
		State&                state;
		Ref<pst::LangElement> el;
		bool                  active_fallback = false;

	public:
		PSTAutomatic(State& state, Ref<pst::LangElement> caller): state(state), el(caller) {}

		PSTAutomatic(const PSTAutomatic&) = delete;

		~PSTAutomatic() {
			if (active_fallback) exitFallback();
		}

		// Useful for debugging:
		//
		// parses one of the available types
		// template<class T>
		// void one([[maybe_unused]]T t) {
		// 	static_assert(sizeof(T) < 0, "parseOne for type `T` is not implemented\n");
		// }

		/**
		 * @brief Parses the expected keyword. Skips on success, logs error on failure.
		 * @param key The expected keyword.
		 */
		PSTAutomatic& one(Keyword key) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.tryEat(key))
				state.logInt(makeBox<tpc::BadKeywordError>(state.getPosition(), key));
			else
				el->addToken(state[-1]);
			return *this;
		}

		/**
		 * @brief Parses the expected Special token. Skips on success, logs error on failure.
		 * @param spec The expected special token.
		 */
		PSTAutomatic& one(Special spec) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.tryEat(spec))
				state.logInt(makeBox<tpc::BadSpecialError>(state.getPosition(), spec));
			else
				el->addToken(state[-1]);
			return *this;
		}

		/**
		 * @brief Parses the expected operator. Skips on success, logs error on failure.
		 * @param op The expected operator.
		 */
		PSTAutomatic& one(Operator op) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.tryEat(op))
				state.logInt(makeBox<tpc::BadOperatorError>(state.getPosition(), op));
			else
				el->addToken(state[-1]);
			return *this;
		}

		/**
		 * @brief Parses a keyword to @p result. Skips on success, logs error on failure.
		 * @param result The place to store the parsed keyword.
		 */
		PSTAutomatic& one(tpc::Keyword* result) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.ctokens().peek().isKeyword()) {
				state.logInt(makeBox<tpc::NoKeywordError>(
					state.getPosition(), state.ctokens().peek().describe()
				));
				*result = Keyword::NotAKeyword;
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
		PSTAutomatic& one(tpc::Identifier* result) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.ctokens().peek().isIdentifier()) {
				state.logInt(makeBox<tpc::NoIdentifierError>(
					state.getPosition(), state.ctokens().peek().describe()
				));
				result->value = base::StrID("<error>");
				return *this;
			}
			el->addToken(state[0]);
			result->position = state.getPosition();
			result->value    = state.tokens().next().getValue();
			return *this;
		}

		/**
		 * @brief Parses a string to @p result. Skips on success, does nothing on failure.
		 * @param result The place to store the parsed string.
		 */
		PSTAutomatic& one(tpc::StringValue* result) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.ctokens().peek().isString()) {
				state.logInt(makeBox<tpc::NoStringError>(state.getPosition()));
				result->value = base::StrID("<error>");
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
		PSTAutomatic& one(tpc::NumericValue* result) {
			PST_AUTOMATIC_SKIP(*this);
			if (!state.ctokens().peek().is(lexer::Token::Type::NumLiteralGroup)) {
				state.logInt(makeBox<tpc::NoNumericValueError>(state.getPosition()));
				result->value = base::StrID("<error>");
				result->type_specifier.reset();
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
		PSTAutomatic& one(MBox<T>* result) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T, base::TemplateStringLiteral name>
		PSTAutomatic& one(AccessInternal<T, name>* result) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T>
		PSTAutomatic& one(AccessInternalAnonymous<T>* result) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T>
		PSTAutomatic& one(base::Optional<MBox<T>>* result) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<LangElement> T, base::TemplateStringLiteral name>
		PSTAutomatic& one(base::Optional<AccessInternal<T, name>>* result) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, T::parse);
			return *this;
		}

		/**
		 * @brief Specialization for optional of identifier wrappers
		 */
		template<std::derived_from<LangElement> T, base::TemplateStringLiteral name>
		requires std::same_as<T, IdentifierWrapper>
		PSTAutomatic& one(base::Optional<AccessInternal<T, name>>* result) {
			PST_AUTOMATIC_SKIP(*this);
			if (state.ctokens().peek().isIdentifier()) with(result, T::parse);
			return *this;
		}

		template<typename... Args>
		PSTAutomatic& assign(Args&&... args) {
			FreeAutomatic::assign(el, std::forward<Args>(args)...);
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
			PST_AUTOMATIC_SKIP(*this);
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
			PST_AUTOMATIC_SKIP(*this);
			MBox<El> result = El::parse(state, std::forward<Args>(args)...);
			assign(sink, std::move(result));
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(MBox<El>* result, Args&&... args) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, base::TemplateStringLiteral name, typename... Args>
		PSTAutomatic& withDef(AccessInternal<El, name>* result, Args&&... args) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(AccessInternalAnonymous<El>* result, Args&&... args) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(base::Optional<MBox<El>>* result, Args&&... args) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, base::TemplateStringLiteral name, typename... Args>
		PSTAutomatic& withDef(base::Optional<AccessInternal<El, name>>* result, Args&&... args) {
			PST_AUTOMATIC_SKIP(*this);
			with(result, El::parse, std::forward<Args>(args)...);
			return *this;
		}

		/**
		 * @brief Call a default parse function with additional arguments and automation.
		 */
		template<std::derived_from<LangElement> El, typename... Args>
		PSTAutomatic& withDef(base::Optional<AccessInternalAnonymous<El>>* result, Args&&... args) {
			PST_AUTOMATIC_SKIP(*this);
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
			PST_AUTOMATIC_SKIP(false);
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
			PST_AUTOMATIC_SKIP(*this);
			if (state.notEmpty() && !state[0].is(Special::Semicolon))
				el->addToken(state.tokens().next());
			return *this;
		}

		/**
		 * @brief Skips any token other then a semicolon
		 */
		PSTAutomatic& skipNotSemicolon() {
			PST_AUTOMATIC_SKIP(*this);
			if (state.notEmpty() && !state[0].is(Special::Semicolon)) state.tokens().skip();
			return *this;
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		PSTAutomatic& goDown() {
			if (state.isSkipping()) {
				state.skipEntry();
				return *this;
			}
			el->addToken(state[0].getSentinelBegin());
			state.goDown();
			return *this;
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		PSTAutomatic& goUpAndSkip() {
			if (state.isSkipping()) {
				if (!state.removeEntry()) return *this;
			}
			state.goUp();
			el->addToken(state[0].getSentinelEnd());
			skipNotSemicolon();
			return *this;
		}

		/**
		 * @brief Setup a fallback for parsing. The fallback is automatically exited when
		 * PSTAutomatic is destructed at the end of the expression.
		 *
		 * Intended usage:
		 * state.parse(el).autoFallbackLen(length).parseOne(...);
		 *
		 * @note Needed when the condition has to be calculated in an hpp file because of templates.
		 * Normally in cpp files the until version should be used
		 */
		PSTAutomatic& autoFallbackLen(u64 length) {
			CORE_ASSERT(
				!active_fallback, "Only one active auto fallback supported in pst automatic"
			);
			active_fallback = true;
			if (state.isSkipping()) {
				state.skipEntry();
				return *this;
			}
			state.setFallback(length);
			return *this;
		}

		/**
		 * @brief Automatic safe conversion version of autoFallbackLen
		 */
		PSTAutomatic& autoFallbackLen(i64 length) {
			return autoFallbackLen(base::safeIntConv<u64>(length));
		}

		/**
		 * @brief Setup a fallback for parsing. The fallback is automatically exited when
		 * PSTAutomatic is destructed at the end of the expression.
		 *
		 * Intended usage:
		 * state.parse(el).autoFallbackUntil<condition>().parseOne(...);
		 */
		template<TokenStreamCondition until>
		PSTAutomatic& autoFallbackUntil() {
			CORE_ASSERT(
				!active_fallback, "Only one active auto fallback supported in pst automatic"
			);
			active_fallback = true;
			return fallbackUntil<until>();
		}

		/**
		 * @brief Sets a simple soft fallback that is only used to save context
		 */
		PSTAutomatic& saveContext() {
			constexpr auto CONST_TRUE = [](const TokenStream&, i64) { return true; };

			return setSoftFallback(CONST_TRUE);
		}

		/**
		 * @brief Setup a fallback for parsing. It limits parsing until exited and resets the
		 * after-error parsing short-cutting when exited.
		 */
		PSTAutomatic& fallbackLen(u64 length) {
			if (state.isSkipping()) {
				state.skipEntry();
				return *this;
			}
			state.setFallback(length);
			return *this;
		}

		/**
		 * @brief Setup a fallback for parsing. It limits parsing until exited and resets the
		 * after-error parsing short-cutting when exited.
		 */
		template<TokenStreamCondition until>
		PSTAutomatic& fallbackUntil() {
			if (state.isSkipping()) {
				state.skipEntry();
				return *this;
			}
			u64 length = state.template countUntil<until>();
			state.setFallback(length);
			return *this;
		}

		/**
		 * @brief Setup a soft fallback for parsing. It limits resets the after-error parsing
		 * short-cutting(using the condition) when exited.
		 */
		PSTAutomatic& setSoftFallback(std::function<TokenStreamCondition> fun) {
			if (state.isSkipping()) {
				state.skipEntry();
				return *this;
			}
			state.setSoftFallback(fun);
			return *this;
		}

		/**
		 * @brief Exit a fallback for parsing. It resets the after-error parsing short-cutting when
		 * exited.
		 */
		PSTAutomatic& exitFallback() {
			if (state.isSkipping()) {
				if (!state.removeEntry()) return *this;
			}
			state.exitFallback();
			return *this;
		}

		/**
		 * @brief Exit a soft fallback for parsing. It resets the after-error parsing short-cutting
		 * when exited.
		 */
		PSTAutomatic& exitSoftFallback() {
			if (state.isSkipping()) {
				if (!state.removeEntry()) return *this;
			}
			state.exitSoftFallback();
			return *this;
		}

		/**
		 * @brief Parses all of the given elements.
		 * @note Forces the first element to be skipped on error if it's a token.
		 */
		template<typename T>
		PSTAutomatic& all(T t) {
			PST_AUTOMATIC_SKIP(*this);
			one(t);
			return *this;
		}

		/**
		 * @brief Parses all of the given elements.
		 * @note Forces the first element to be skipped on error if it's a token.
		 */
		template<typename T, typename... Q>
		PSTAutomatic& all(T t, Q... q) {
			PST_AUTOMATIC_SKIP(*this);
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
			one(t);
		}

		/**
		 * @brief Parses all of the given elements.
		 */
		template<typename T, typename... Q>
		void parseRest(T t, Q... q) {
			one(t);
			parseRest(q...);
		}
	};

	/**
	 * @brief Setup a fallback for parsing. It limits parsing until exited and resets the
	 * after-error parsing short-cutting when exited.
	 * @note Should not be normally used, is used in situations where there is no elements that is
	 * currently being parsed. For example in statement parsing.
	 */
	void fallbackLen(LangParserState& state, u64 length);

	/**
	 * @brief Exit a fallback for parsing. It resets the after-error parsing short-cutting when
	 * exited.
	 * @note Should not be normally used, is used in situations where there is no elements that is
	 * currently being parsed. For example in statement parsing.
	 */
	void exitFallback(LangParserState& state);

	/**
	 * @brief Setup a soft fallback for parsing. It resets the
	 * after-error parsing short-cutting (using the condition) when exited.
	 * @note Should not be normally used, is used in situations where there is no elements that is
	 * currently being parsed. For example in statement parsing.
	 */
	void setSoftFallback(LangParserState& state, TokenStreamCondition fun);

	/**
	 * @brief Exit a soft fallback for parsing. It resets the after-error parsing short-cutting
	 * (using the saved condition) when exited.
	 * @note Should not be normally used, is used in situations where there is no elements that is
	 * currently being parsed. For example in statement parsing.
	 */
	void exitSoftFallback(LangParserState& state);
}
