// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../../state/lang_state_unmethods.hpp"
#include "meta.hpp"

#include <diagnostic/source_position.hpp>
#include <string_id/string_id.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/token_stream.hpp>

namespace pst {
	/**
	 * @brief Class that keeps an expression with information whether it's a top-level expression.
	 *
	 * It's intended to be the only holder that is visible to further stages of compilation.
	 */
	class ExprHolder: public NotStmt {
		THIS_CLASS(ExprHolder);
		PARENT_CLASS(NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(expr, ExprElement);
		friend void internal::parseExprIntoHolder(
			LangParserState&, Ref<ExprHolder>, ExprParseFun, u64
		);

	public:
		ELEMENT_CLONE_DECL(ExprHolder);

		explicit ExprHolder(LangElementConstructionData state): NotStmt(state) {
			this->element_kind = ElementKind::ExprHolder;
		}

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<ExprElement> getExpr() const {
			return expr.give();
		}

		[[nodiscard]]
		virtual bool isTopLevel() const
			= 0;
	};

	/**
	 * @brief Collects different parsing entries.
	 *
	 * @note The naming scheme `until_____` that is used for these functions can be viewed as
	 * `is____` when considering them in a vacuum. `until_____` is used for them to be more readable
	 * as template arguments in expression holders as this is their goal.
	 */
	class ExprParserHelper {
	public:
		ExprParserHelper() = delete;
		static bool untilUniversalNoBlockEnd(const TokenStream&, i64);
		static bool untilUniversalEnd(const TokenStream&, i64);
		static bool untilUniversalAllowCommaNoBlockEnd(const TokenStream&, i64);
		static bool untilUniversalAllowCommaEnd(const TokenStream&, i64);
		static bool untilSemicolon(const TokenStream&, i64);
		static bool untilForTypeEnd(const TokenStream&, i64);
		static bool untilExtendsEnd(const TokenStream&, i64);

		static MBox<ExprElement> parseAssignment(LangParserState& state);
		static MBox<ExprElement> parseComma(LangParserState& state);
		static MBox<ExprElement> parseTernary(LangParserState& state);
	};

	/**
	 * @brief Template for defining expression parsing entry points.
	 * Its intended usage is to derive holder after this template:
	 * `class Holder: ExprHolderTemplate<Holder, parser_fun, top_level>`
	 *
	 * @tparam Self - Class of the holder, used for the correct return type of parse.
	 * @tparam parseFun - The parsing function that parses the inner expression.
	 * @tparam until - Condition for the end of parsing.
	 * @tparam TOP_LEVEL - Whether the holder holds a top-level expression, for example some lists
	 * shouldn't.
	 */
	template<typename Self, ExprParseFun parseFun, TokenStreamCondition until, bool TOP_LEVEL = true>
	class ExprHolderTemplate: public ExprHolder {
		THIS_CLASS(ExprHolderTemplate);
		PARENT_CLASS(ExprHolder);

	public:
		using ExprHolder::ExprHolder;

		ELEMENT_CLONE_DECL(ExprHolderTemplate);

		static MBox<Self> parse(LangParserState& state) {
			auto out = makeBox<Self>(state);

			auto length = internal::getTokenStream(state).countUntil<until>();
			internal::parseExprIntoHolder(state, out.refMut(), parseFun, length);
			return out;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return TOP_LEVEL ? "Top Level Expression" : "Expression Holder";
		}

		[[nodiscard]]
		bool isTopLevel() const override {
			return TOP_LEVEL;
		}
	};

	/**
	 * @brief `UniversalExprHolder` that ends at a block instead of parsing it as a block
	 * expression.
	 */
	class UniversalNoBlockExprHolder final:
		  public ExprHolderTemplate<
			  UniversalNoBlockExprHolder,
			  ExprParserHelper::parseTernary,
			  ExprParserHelper::untilUniversalNoBlockEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(UniversalNoBlockExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~UniversalNoBlockExprHolder() final = default;
	};

	/**
	 * @brief The default entry point to expression parsing that doesn't allow comma expressions
	 * top-level.
	 */
	class UniversalExprHolder final:
		  public ExprHolderTemplate<
			  UniversalExprHolder,
			  ExprParserHelper::parseTernary,
			  ExprParserHelper::untilUniversalEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(UniversalExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~UniversalExprHolder() final = default;
	};

	/**
	 * @brief The version of `UniversalExprHolder` that isn't top-level.
	 */
	class UniversalExprHolderLowerLevel final:
		  public ExprHolderTemplate<
			  UniversalExprHolderLowerLevel,
			  ExprParserHelper::parseTernary,
			  ExprParserHelper::untilUniversalEnd,
			  false> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(UniversalExprHolderLowerLevel, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~UniversalExprHolderLowerLevel() final = default;
	};

	/**
	 * @brief Secondary entry point to expression parsing that allows comma expressions but doesn't
	 * allow for assignment expressions top-level.
	 */
	class CommaExprHolder final:
		  public ExprHolderTemplate<
			  CommaExprHolder,
			  ExprParserHelper::parseComma,
			  ExprParserHelper::untilUniversalAllowCommaEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CommaExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~CommaExprHolder() final = default;
	};

	/**
	 * @brief `CommaExprHolder` that ends at a block instead of parsing it as a block expression.
	 *
	 * Used for types, so that a missing `=` in `fun f() -> T { ... }` does not make the body a
	 * part of the type.
	 */
	class CommaNoBlockExprHolder final:
		  public ExprHolderTemplate<
			  CommaNoBlockExprHolder,
			  ExprParserHelper::parseComma,
			  ExprParserHelper::untilUniversalAllowCommaNoBlockEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CommaNoBlockExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~CommaNoBlockExprHolder() final = default;
	};

	/**
	 * @brief Tertiary and most broad entry point to expression parsing that allows assignment
	 * expressions top-level. Allows for block expressions.
	 */
	class AssignmentExprHolder final:
		  public ExprHolderTemplate<
			  AssignmentExprHolder,
			  ExprParserHelper::parseAssignment,
			  ExprParserHelper::untilSemicolon,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(AssignmentExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~AssignmentExprHolder() final = default;
	};

	/**
	 * @brief Expression parsing entry point for type in for statement, i.e.:
	 * `for (iter: this-expr in range) {...}`
	 */
	class ForTypeExprHolder final:
		  public ExprHolderTemplate<
			  ForTypeExprHolder,
			  ExprParserHelper::parseComma,
			  ExprParserHelper::untilForTypeEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ForTypeExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~ForTypeExprHolder() final = default;
	};

	/**
	 * @brief Expression parsing entry point for Implements list.
	 */
	class ExtendsExprHolder final:
		  public ExprHolderTemplate<
			  ExtendsExprHolder,
			  ExprParserHelper::parseTernary,
			  ExprParserHelper::untilExtendsEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExtendsExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~ExtendsExprHolder() final = default;
	};

	/**
	 * @brief Expression parsing entry point for Implements list.
	 */
	class ImplementsElementExprHolder final:
		  public ExprHolderTemplate<
			  ImplementsElementExprHolder,
			  ExprParserHelper::parseTernary,
			  ExprParserHelper::untilUniversalNoBlockEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ImplementsElementExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~ImplementsElementExprHolder() final = default;
	};

	/**
	 * @brief Expression parsing entry point for Implements list.
	 */
	class ValuePatternExprHolder final:
		  public ExprHolderTemplate<
			  ValuePatternExprHolder,
			  ExprParserHelper::parseTernary,
			  ExprParserHelper::untilUniversalEnd,
			  true> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ValuePatternExprHolder, ExprHolderTemplate);

	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~ValuePatternExprHolder() final = default;
	};
}
