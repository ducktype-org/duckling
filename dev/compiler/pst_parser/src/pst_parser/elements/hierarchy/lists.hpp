#pragma once

#include "meta.hpp"

namespace pst {
	/**
	 * @brief General Element representing a list of Elements.
	 *
	 * @tparam ListElements - Kept Elements, has to have precise length parse like Expr
	 * @tparam getName - List name getter for errors.
	 * @tparam Container - Vector-like container of SubElements with emplace_back. Possibly with
	 * other condition because of iteration.
	 */
	template<
		class ListElements,
		GetName getName,
		class Container = std::vector<AccessInternal<ListElements>>>
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
				nullAwareDprint(x, out);
				out << ",";
			}
			out << "]";
		}
	};

	/**
	 * @brief Function declaration parameter list.
	 */
	class ParamList final: public List<FunParam, detail::NameGetters::parameterList> {
	public:
		explicit ParamList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::ParamList;
		}

		static MBox<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};

	/**
	 * @brief Class implements list.
	 */
	class ImplementsList final:
		  public List<UniversalExprHolder, detail::NameGetters::inheritanceList> {
	public:
		explicit ImplementsList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};

	/**
	 * @brief Attribute argument list.
	 */
	class AtrArgList final:
		  public List<UniversalExprHolder, detail::NameGetters::attributeArgList> {
	public:
		explicit AtrArgList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};

	/**
	 * @brief c++-like class constructor initialization list.
	 *
	 * @note It's probably going to be deprecated
	 */
	class InitList final: public List<UniversalExprHolder, detail::NameGetters::classInitList> {
	public:
		explicit InitList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<InitList> parse(LangParserState& state);

		~InitList() final = default;
	};

	/**
	 * @brief Call argument list.
	 */
	class CallList final:
		  public List<UniversalExprHolderLowerLevel, detail::NameGetters::callList> {
	public:
		explicit CallList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::CallList;
		}

		static MBox<CallList> parse(LangParserState& state);

		~CallList() final = default;
	};

	/**
	 * @brief Template initialization list.
	 */
	class TemplateList final:
		  public List<UniversalExprHolderLowerLevel, detail::NameGetters::templateList> {
	public:
		explicit TemplateList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
