#pragma once

#include "meta.hpp"

namespace pst {
	/**
	 * @brief General Element representing a list of Elements.
	 *
	 * @tparam ListElements - Kept Elements, has to have precise length parse like Expr
	 * @tparam Self - Inheriting class type for construction purposes
	 * @tparam NON_EMPTY - Should empty list be an error.
	 * @tparam BRACKETS - expected brackets or None if not expected
	 * @tparam isSeparator - Separator should always be skip-able with one skip.
	 * @tparam isEnding - Check for successful ending.
	 * @tparam getName - List name getter for errors.
	 * @tparam Container - Vector-like container of SubElements with emplace_back. Possibly with
	 * other condition because of iteration.
	 */
	template<
		class ListElements,
		GetName getName,
		class Container = std::vector<ParserRef<ListElements>>>
	class List: public NotStmt {
	protected:
		Container elements;

	public:
		friend class ListParsingTemplate;

		DECLARE_CONST_ELEMENT_ITERATOR(elements, ListElements)

		[[nodiscard]]
		usize size() const {
			return elements.size();
		}

		explicit List(const dia::SourcePosition& position): NotStmt(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return getName() + " list";
		}

		void dprint(std::ostream& out) const final {
			out << "[";
			for (auto& x: elements) {
				tpc::nullAwareDprint(x, out);
				out << ",";
			}
			out << "]";
		}
	};

	class ParamList final: public List<FunParam, detail::NameGetters::parameterList> {
	public:
		explicit ParamList(const dia::SourcePosition& pos): List(pos) {}

		static ParserRef<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};

	class ImplementsList final: public List<ExprElement, detail::NameGetters::inheritanceList> {
	public:
		explicit ImplementsList(const dia::SourcePosition& pos): List(pos) {}

		static ParserRef<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};

	class AtrArgList final: public List<ExprElement, detail::NameGetters::attributeArgList> {
	public:
		explicit AtrArgList(const dia::SourcePosition& pos): List(pos) {}

		static ParserRef<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};

	class InitList final: public List<ExprElement, detail::NameGetters::classInitList> {
	public:
		explicit InitList(const dia::SourcePosition& pos): List(pos) {}

		static ParserRef<InitList> parse(LangParserState& state);

		~InitList() final = default;
	};
}
