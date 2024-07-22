#pragma once

#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/automatic.hpp>
#include <base/strongly_typed_id.hpp>

#include <variant>

namespace pst {
	class Import;

	using tpc::ParserBorrowRef;
	using tpc::ParserCBorrowRef;
	using tpc::ParserRef;

	STRONG_TYPEDEF_ID(PstID);

	template<typename State>
	class PSTAutomatic;

	/**
	 * @brief Base Element for all of the PST elements.
	 */
	class RiftElement: public tpc::Element {
	public:
		using Child      = ParserBorrowRef<RiftElement>;
		using ConstChild = ParserCBorrowRef<RiftElement>;

		using SubToken = base::c_borrow_ptr<tpc::Token>;

		using SubElement      = std::variant<SubToken, Child>;
		using ConstSubElement = std::variant<SubToken, ConstChild>;

		explicit RiftElement(const dia::SourcePosition& position):
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
			return std::ranges::ref_view(sub_elements) | transform(visitConstChild);
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
		ParserCBorrowRef<RiftElement> getParent() const {
			return parent;
		}

		/**
		 * @brief Returns a string of element type.
		 *
		 * Mostly for debugging and visualization.
		 */
		[[nodiscard]]
		virtual std::string elementType() const {
			return "Element";
		}

		template<typename X>
		friend class PSTAutomatic;

	protected:
		dia::SourcePosition          source_position;
		std::vector<SubElement>      sub_elements;
		ParserBorrowRef<RiftElement> parent;

		void addToken(const tpc::Token& token);
		void addToken(const base::unique_ptr<tpc::Token>& token);
		void addToken(base::c_borrow_ptr<tpc::Token> token);

		template<std::derived_from<RiftElement> El>
		void addChild(base::Optional<ParserRef<El>>& el) {
			if (el) addChild(el.value().borrow());
		}

		void addChild(ParserBorrowRef<RiftElement> child);

		template<typename T>
		void addChild(ParserRef<T>& child) {
			addChild(child.borrow_mut());
		}

		/**
		 * @brief Updates the position to include the end of the given position.
		 */
		void setLastToken(dia::SourcePosition pos);

		void setParent(ParserBorrowRef<RiftElement> parent) { this->parent = parent; }

	private:
		PstID id = PstID::next();
	};

	using ImportType = tpc::ParserCBorrowRef<pst::Import>;
}

ID_STD_HASH(::pst::PstID);
