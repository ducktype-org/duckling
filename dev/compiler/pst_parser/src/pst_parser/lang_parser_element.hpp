#pragma once

#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/automatic.hpp>
#include <base/strongly_typed_id.hpp>
#include <base/ref.hpp>
#include <base/box.hpp>

#include <variant>

#include "element_kind.hpp"

namespace pst {
	class Import;

	STRONG_TYPEDEF_ID(PstID);

	template<typename State>
	class PSTAutomatic;

	/**
	 * @brief Base Element for all of the PST elements.
	 */
	class LangElement: public tpc::Element {
	public:
		using Child      = Ref<LangElement>;
		using ConstChild = CRef<LangElement>;

		using SubToken = base::c_borrow_ptr<tpc::Token>;

		using SubElement      = std::variant<SubToken, Child>;
		using ConstSubElement = std::variant<SubToken, ConstChild>;

		explicit LangElement(const dia::SourcePosition& position):
			  source_position(position),
			  id(PstID::next()) {}

		/**
		 * @brief Position covering the whole element
		 */
		[[nodiscard]]
		const dia::SourcePosition& getSourcePosition() const;

	private:
		/**
		 * @brief Helper function for filtering variants
		 */
		template<typename T, typename U>
		static bool holds(const U& el) {
			return std::holds_alternative<T>(el);
		}

		/**
		 * @brief Helper function for extracting from variants
		 */
		template<typename T, typename U>
		static T choose(const U& el) {
			return std::get<T>(el);
		}

		/**
		 * @brief Helper overload properly handling the const-ness of Children.
		 *
		 * If the element is const then the view should give access to const elements.
		 */
		struct getConstChild {
			ConstSubElement operator()(const SubToken& token) { return token; }

			ConstSubElement operator()(const Child& child) { return ConstChild(child); }
		};

		/**
		 * @brief Helper for properly converting sub-element to const.
		 */
		static ConstSubElement visitConstChild(const SubElement& element) {
			return std::visit(getConstChild(), element);
		}

	public:
		/**
		 * @brief Non-const view all sub-elements.
		 */
		[[nodiscard]]
		auto viewSubElements() {
			using namespace std::views;
			return std::ranges::ref_view(sub_elements);
		}

		/**
		 * @brief Non-const view all child elements.
		 */
		[[nodiscard]]
		auto viewChildren() {
			using namespace std::views;
			return viewSubElements() | filter(holds<Child, SubElement>)
			     | transform(choose<Child, SubElement>);
		}

		/**
		 * @brief Non-const view all child tokens.
		 */
		[[nodiscard]]
		auto viewTokens() {
			using namespace std::views;
			return viewSubElements() | filter(holds<SubToken, SubElement>)
			     | transform(choose<SubToken, SubElement>);
		}

		/**
		 * @brief Const view all sub-elements.
		 */
		[[nodiscard]]
		auto viewSubElements() const {
			using namespace std::views;
			return sub_elements | transform(visitConstChild);
		}

		/**
		 * @brief Const view all child elements.
		 */
		[[nodiscard]]
		auto viewChildren() const {
			using namespace std::views;
			return viewSubElements() | filter(holds<ConstChild, ConstSubElement>)
			     | transform(choose<ConstChild, ConstSubElement>);
		}

		/**
		 * @brief Const view all child tokens.
		 */
		[[nodiscard]]
		auto viewTokens() const {
			using namespace std::views;
			return viewSubElements() | filter(holds<SubToken, ConstSubElement>)
			     | transform(choose<SubToken, ConstSubElement>);
		}

		[[nodiscard]]
		PstID getID() const {
			return id;
		}

		/**
		 * @return Whether an element is just a statement aggregate.
		 * As of 30.05.2024 there are 3 statement aggregates:
		 * * CodeBlock
		 * * CodeBlockOrStmt
		 * * TopLevel
		 */
		[[nodiscard]]
		virtual bool isStatementAggregate() const {
			return false;
		}

		/**
		 * @return if element is a statements
		 */
		[[nodiscard]]
		virtual bool isStatement() const {
			return false;
		}

		[[nodiscard]]
		auto getParent() const {
			return parent;
		}

		/**
		 * @brief Returns a string of element type.
		 *
		 * Mostly for debugging and visualization.
		 * @todo add element type the stringifiable enum
		 * @note it is used by helios as a hacky way to check if given element in an expression
		 */
		[[nodiscard]]
		std::string elementType() const override {
			return "Element";
		}

		/**
		 * @brief Returns the kind of the element.
		 */
		[[nodiscard]]
		ElementKind getElementKind() const {
			return kind;
		}

		template<typename X>
		friend class PSTAutomatic;

	protected:
		dia::SourcePosition              source_position;
		std::vector<SubElement>          sub_elements;
		base::Optional<Ref<LangElement>> parent;
		ElementKind                      kind = ElementKind::KindNotSet;	

		void addToken(const tpc::Token& token);
		void addToken(const base::unique_ptr<tpc::Token>& token);
		void addToken(base::c_borrow_ptr<tpc::Token> token);

		template<std::derived_from<LangElement> El>
		void addChild(MRef<El> el) {
			auto opt = el.toOpt();
			if (opt) addChild(opt.value());
		}

		void addChild(MRef<LangElement> child);

		template<typename T>
		void addChild(MBox<T>& child) {
			addChild(child.refMut());
		}

		/**
		 * @brief Updates the position to include the end of the given position.
		 */
		void setLastToken(dia::SourcePosition pos);

		/**
		 * @brief Updates the position to include the beginning of the given position.
		 */
		void setFirstToken(dia::SourcePosition pos);

		void setParent(Ref<LangElement> parent) { this->parent.emplace(parent); }

	private:
		PstID id = PstID::next();
	};

	using ImportType = CRef<pst::Import>;
}

ID_STD_HASH(::pst::PstID);
