#pragma once

#include "../lang_state_unmethods.hpp"
#include "meta.hpp"

#include <base/string_id.hpp>

#include <diagnostic/source_position.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/token_stream.hpp>

namespace pst {
	/**
	 * @brief Class that keeps an expression with information whether it's a top-level expression.
	 */
	class ExprHolder: public NotStmt {
	protected:
		AccessInternal<ExprElement> expr;
		friend void internal::parseExprIntoHolder(LangParserState&, Ref<ExprHolder>, ExprParseFun);

	public:
		explicit ExprHolder(const dia::SourcePosition& pos): NotStmt(pos) {
			this->element_kind = ElementKind::ExprHolder;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Top Level Expression";
		}

		void dprint(std::ostream& out) const final;

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
	 */
	class ExprParserHelper {
	public:
		ExprParserHelper() = delete;
		static MBox<ExprElement> parseUniversal(LangParserState& state);
		static MBox<ExprElement> parseComma(LangParserState& state);
		static MBox<ExprElement> parseAssignment(LangParserState& state);
		static MBox<ExprElement> parseForType(LangParserState& state);
	};

	/**
	 * @brief Template for defining expression parsing entry points.
	 * Its intended usage is to derive holder after this template:
	 * `class Holder: ExprHolderTemplate<Holder, parser_fun, top_level>`
	 *
	 * @tparam Self - Class of the holder, used for the correct return type of parse.
	 * @tparam parseFun - The parsing function that parses the inner expression.
	 * @tparam TOP_LEVEL - Whether the holder holds a top-level expression, for example some lists
	 * shouldn't.
	 */
	template<typename Self, ExprParseFun parseFun, bool TOP_LEVEL = true>
	class ExprHolderTemplate: public ExprHolder {
	public:
		using ExprHolder::ExprHolder;

		static MBox<Self> parse(LangParserState& state) {
			auto position = internal::getPosition(state);
			auto out      = makeBox<Self>(position);

			internal::parseExprIntoHolder(state, out.refMut(), parseFun);
			return out;
		}

		[[nodiscard]]
		bool isTopLevel() const override {
			return TOP_LEVEL;
		}
	};

	/**
	 * @brief The default entry point to expression parsing that doesn't allow comma expressions
	 * top-level
	 */
	class UniversalExprHolder final:
		  public ExprHolderTemplate<UniversalExprHolder, ExprParserHelper::parseUniversal, true> {
	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~UniversalExprHolder() final = default;
	};

	/**
	 * @brief The version of the default entry point that isn't top-level
	 */
	class UniversalExprHolderLowerLevel final:
		  public ExprHolderTemplate<
			  UniversalExprHolderLowerLevel,
			  ExprParserHelper::parseUniversal,
			  false> {
	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~UniversalExprHolderLowerLevel() final = default;
	};

	/**
	 * @brief Secondary entry point to expression parsing that allows comma expressions but doesn't
	 * allow for assignment expressions top-level
	 */
	class CommaExprHolder final:
		  public ExprHolderTemplate<CommaExprHolder, ExprParserHelper::parseComma, true> {
	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~CommaExprHolder() final = default;
	};

	/**
	 * @brief Tertiary and most broad entry point to expression parsing that allows assignment
	 * expressions top-level.
	 */
	class AssignmentExprHolder final:
		  public ExprHolderTemplate<AssignmentExprHolder, ExprParserHelper::parseAssignment, true> {
	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~AssignmentExprHolder() final = default;
	};

	/**
	 * @brief Expression parsing entry point for type in for statement, i.e.:
	 * `for (iter: this-expr in range) {...}`
	 */
	class ForTypeExprHolder final:
		  public ExprHolderTemplate<ForTypeExprHolder, ExprParserHelper::parseForType, true> {
	public:
		using ExprHolderTemplate::ExprHolderTemplate;
		~ForTypeExprHolder() final = default;
	};
}
