#pragma once

#include "access.hpp"
#include "element_kind.hpp"
#include "elements/elements_list.hpp"
#include "pst_id.hpp"

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/variant.hpp>

#include <lexer/token.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>

#include <ranges>
#include <variant>

namespace pst {
	class Import;

	template<typename State>
	class PSTAutomatic;

	class PstVisitor;

	/**
	 * @brief This is a simple text implementation of element path that might still have some
	 * conflicts
	 *
	 * It's supposed to uniquely identify elements in a parsed tree while ignoring some
	 * changes(mostly symbols with different names changing).
	 *
	 * The path is constructed from words periods and brackets:
	 *  - Capitalized words signify element type
	 *  - lowercase words signify accessors such as left, right, block
	 *  - numbers in brackets signify which element it signifies
	 *
	 * @note The current implementation is non-optimal but for now it should suffice. Currently it
	 * does a lot of copying strings that might get better with some references or something similar.
	 *
	 * @todo Add source file/path information
	 */
	struct ElementPath final {
		ElementPath() = default;

		ElementPath(const ElementPath& parent, std::string_view ext):
			  elements(parent.elements.begin(), parent.elements.end()) {
			elements.emplace_back(ext);
		}

		/**
		 * @brief Return the path as a string by joining with '.'
		 */
		[[nodiscard]]
		std::string str() const;

	private:
		std::vector<std::string> elements;
	};

	/**
	 * @brief Base Element for all of the PST elements.
	 */
	class LangElement: public tpc::Element {
	public:
		using SubToken = base::CRef<lexer::Token>;

		template<std::derived_from<LangElement>, std::derived_from<LangElement>>
		friend class PST;

		friend class Stmt;

	protected:
		static base::HashMap<u64, AccessLocked<LangElement>> pst_id_map;

		using InternalChild = Ref<LangElement>;

		struct InternalNamedChild final {
			std::string      name;
			Ref<LangElement> element;
		};

		using InternalSubElement = std::variant<SubToken, InternalChild, InternalNamedChild>;

	public:
		using Child = AccessLocked<LangElement>;

		struct NamedChild final {
			std::string               name;
			AccessLocked<LangElement> element;
		};

		using SubElement = std::variant<SubToken, Child, NamedChild>;

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

		/**
		 * @brief Calculates the Element paths for children of this element, has to be overriden for
		 * elements that have unnamed children.
		 */
		virtual void calcElementPathsRecursive();

		/**
		 * @brief Calculates Element paths for this Element and children.
		 */
		void calcElementPaths(const ElementPath& path) {
			element_path = { path, elementType() };
			calcElementPathsRecursive();
		}

		// /**
		// * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		// * from other elements
		// */
		// template<typename Element, base::TemplateStringLiteral name>
		// void calcChildPath(AccessInternal<Element, name>& access_ref, const ElementPath& path)
		// const { if (auto ref = access_ref.internalMut()) ref->calcElementPaths(path);
		// }

		/**
		 * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		 * from other elements
		 */
		template<typename Element>
		void calcChildPath(AccessInternalAnonymous<Element>& access_ref, const ElementPath& path)
			const {
			if (auto ref = access_ref.internalMut()) ref->calcElementPaths(path);
		}

		/**
		 * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		 * from other elements
		 */
		template<typename Element, base::TemplateStringLiteral name>
		void calcNamedChildPath(AccessInternal<Element, name>& access_ref, const ElementPath& path)
			const {
			if (auto ref = access_ref.internalMut())
				ref->calcElementPaths({ path, std::string(name.value) });
		}

		/**
		 * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		 * from other elements
		 */
		template<typename Element>
		void calcIndexedListChildPath(
			std::span<AccessInternalAnonymous<Element>> vec, const ElementPath& path
		) const {
			for (usize i = 0; i < vec.size(); i++) {
				ElementPath child_path(path, std::format("[{}]", i));
				calcChildPath(vec[i], child_path);
			}
		}

		/**
		 * @brief Calculates Element paths for a completely ordered list of statements.
		 */
		void calcOrderedListChildPath(std::vector<AccessInternalAnonymous<Stmt>>&, const ElementPath&);

	public:
		/**
		 * @brief View all sub-elements.
		 */
		[[nodiscard]]
		auto viewSubElements() const {
			using namespace std::views;

			constexpr auto get_locked = [](const InternalSubElement& t) -> SubElement {
				if (base::holds<InternalChild>(t)) {
					return Child(std::get<InternalChild>(t));
				} else if (base::holds<InternalNamedChild>(t)) {
					auto& [name, inter] = std::get<InternalNamedChild>(t);
					return NamedChild{ .name = name, .element = { inter } };
				} else {
					return SubToken(std::get<SubToken>(t));
				}
			};

			return std::ranges::ref_view(sub_elements) | transform(get_locked);
		}

		/**
		 * @brief View all child elements.
		 */
		[[nodiscard]]
		auto viewChildren() const {
			using namespace std::views;

			constexpr auto is_child = [](const SubElement& t) -> bool {
				return base::holds<Child>(t) || base::holds<NamedChild>(t);
			};
			constexpr auto strip_name = [](const SubElement& t) -> const Child {
				if (base::holds<NamedChild>(t))
					return std::get<NamedChild>(t).element;
				else
					return std::get<Child>(t);
			};

			return viewSubElements() | filter(is_child) | transform(strip_name);
		}

		/**
		 * @brief View all child tokens.
		 */
		[[nodiscard]]
		auto viewTokens() const {
			using namespace std::views;
			return viewSubElements() | filter(base::holds<SubToken, SubElement>)
			     | transform(base::choose<SubToken, SubElement>);
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

		[[nodiscard]]
		const ElementPath& getElementPath() const {
			CORE_ASSERT(element_path.has_value(), "element path not calculated");
			return element_path.value();
		}

		virtual void acceptVisitor(PstVisitor& visitor) const;

		template<typename X>
		friend class PSTAutomatic;

	protected:
		dia::SourcePosition                       source_position;
		std::vector<InternalSubElement>           sub_elements;
		base::Optional<AccessLocked<LangElement>> parent;
		base::Optional<ElementPath>               element_path;

		/**
		 * @brief Kind of the element.
		 * @note This is mostly for HELIOS to decide how to create scopes.
		 */
		ElementKind element_kind = ElementKind::KindNotSet;

		void addToken(const lexer::Token& token);
		void addToken(const Box<lexer::Token>& token);
		void addToken(CRef<lexer::Token> token);

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

		template<std::derived_from<LangElement> El>
		void addNamedChild(const std::string& name, MRef<El> el) {
			auto opt = el.toOpt();
			if (opt) addNamedChild(name, opt.value());
		}

		void addNamedChild(const std::string& name, MRef<LangElement> child);

		template<typename T>
		void addNamedChild(const std::string& name, MBox<T>& child) {
			addNamedChild(name, child.refMut());
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
