#pragma once

#include "access.hpp"
#include "element_kind.hpp"
#include "pst_id.hpp"

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>

#include <ranges>
#include <variant>

namespace pst {
	class Import;

	template<typename State>
	class PSTAutomatic;

	class PstVisitor;

	/**
	 * @brief Base Element for all of the PST elements.
	 */
	class LangElement: public tpc::Element {
	private:
		static base::HashMap<u64, AccessLocked<LangElement>> pst_id_map;

	public:
		using Child = AccessLocked<LangElement>;

		using SubToken = base::CRef<tpc::Token>;

		using SubElement = std::variant<SubToken, Child>;

		explicit LangElement(const dia::SourcePosition& position):
			  source_position(position),
			  id(PstID::next()) {
			pst_id_map.emplace(id, AccessLocked<LangElement>(CRef<LangElement>(this)));
		}

		LangElement(const LangElement&) = delete;
		LangElement(LangElement&&)      = delete;

		/**
		 * @brief Position covering the whole element
		 */
		[[nodiscard]]
		const dia::SourcePosition& getSourcePosition() const;

		/**
		 * @brief Get pst node the by id. Throws on non-existent id.
		 */
		[[nodiscard]]
		static AccessLocked<LangElement> getByID(u64);

	protected:
		void dprintPrefix(std::ostream& out) const override {
			tpc::Element::dprintPrefix(out);
			out << R"("position": )";
			source_position.printToJson(out);
			out << ", ";
		}

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

	public:
		/**
		 * @brief View all sub-elements.
		 */
		[[nodiscard]]
		auto viewSubElements() const {
			using namespace std::views;
			return std::ranges::ref_view(sub_elements);
		}

		/**
		 * @brief View all child elements.
		 */
		[[nodiscard]]
		auto viewChildren() const {
			using namespace std::views;
			return viewSubElements() | filter(holds<Child, SubElement>)
			     | transform(choose<Child, SubElement>);
		}

		/**
		 * @brief View all child tokens.
		 */
		[[nodiscard]]
		auto viewTokens() const {
			using namespace std::views;
			return sub_elements | filter(holds<SubToken, SubElement>)
			     | transform(choose<SubToken, SubElement>);
		}

		[[nodiscard]]
		PstID getID() const {
			return id;
		}

		/**
		 * @return Whether an element is just a statement aggregate.
		 * As of 11.12.2024 there are 4 statement aggregates:
		 * * CodeBlock
		 * * CodeBlockOrStmt
		 * * TopLevel
		 * * ClassBlock
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
		base::Optional<AccessLocked<LangElement>> getParent() const;

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
		 * This is mostly to decide how HELIOS will create scopes for this element.
		 * We might change it to "ScopeKind" in the future.
		 * For now we keep logic of deciding on ScopeKinds in HELIOS
		 * for consistency.
		 */
		[[nodiscard]]
		ElementKind getElementKind() const {
			CORE_ASSERT(
				element_kind != ElementKind::KindNotSet,
				base::strConcat("Element kind not set. Element type: ", elementType())
			);
			return element_kind;
		}

		virtual void acceptVisitor(PstVisitor& visitor) const;

		template<typename X>
		friend class PSTAutomatic;

	protected:
		dia::SourcePosition                       source_position;
		std::vector<SubElement>                   sub_elements;
		base::Optional<AccessLocked<LangElement>> parent;

		/**
		 * @brief Kind of the element.
		 * @note This is mostly for HELIOS to decide how to create scopes.
		 */
		ElementKind element_kind = ElementKind::KindNotSet;

		void addToken(const tpc::Token& token);
		void addToken(const Box<tpc::Token>& token);
		void addToken(CRef<tpc::Token> token);

		template<std::derived_from<LangElement> El>
		void addChild(MCRef<El> el) {
			auto opt = el.toOpt();
			if (opt) addChild(opt.value());
		}

		void addChild(MCRef<LangElement> child);

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

		void setParent(Ref<LangElement> parent) { this->parent = { parent }; }

	private:
		PstID id = PstID::next();
	};

	using ImportType = CRef<pst::Import>;
}
