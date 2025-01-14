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
	template<class ListElements, GetName getName, class Container = std::vector<MBox<ListElements>>>
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
		explicit ParamList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::ParamList;
		}

		static MBox<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};

	class ImplementsList final:
		  public List<UniversalExprHolder, detail::NameGetters::inheritanceList> {
	public:
		explicit ImplementsList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};

	class AtrArgList final:
		  public List<UniversalExprHolder, detail::NameGetters::attributeArgList> {
	public:
		explicit AtrArgList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};

	class InitList final: public List<UniversalExprHolder, detail::NameGetters::classInitList> {
	public:
		explicit InitList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<InitList> parse(LangParserState& state);

		~InitList() final = default;
	};

	class CallList final:
		  public List<UniversalExprHolderLowerLevel, detail::NameGetters::callList> {
	public:
		explicit CallList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<CallList> parse(LangParserState& state);

		~CallList() final = default;
	};

	class TemplateList final:
		  public List<UniversalExprHolderLowerLevel, detail::NameGetters::templateList> {
	public:
		explicit TemplateList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
