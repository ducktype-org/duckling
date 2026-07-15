#pragma once

#include "access.hpp"
#include "cloning_decl.hpp"
#include "element_kind.hpp"
#include "elements/elements_list.hpp"
#include "elements/lang_state_unmethods.hpp"
#include "pst_config.hpp"
#include "pst_id.hpp"
#include "source_position_locked.hpp"

#include <concurrent/base/collections/hash_map.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/anycast.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <hashing/component_hash.hpp>
#include <lexer/token.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>

#include <any>
#include <ranges>
#include <variant>

namespace dia_int {
	class StablePosition;
}

namespace pst {
	class Import;

	template<typename State>
	class PSTAutomatic;

	class PstVisitor;


	class LangParserState;

	/**
	 * @brief Additional data optionally stored in root elements of the PST.
	 * @TODO: #2397 probably move or remove it.
	 */
	struct AdditionalRootData final {
		struct MacroExpansionParent final {
			/**
			 * @brief Optional source element of macro expansion the PST was generated from.
			 */
			AccessLocked<LangElement> expand_element;
		};

		struct ModuleParent final {
			/**
			 * @brief frontend::ModuleID the PST was generated from.
			 */
			std::any module_id;
		};

		/**
		 * @brief Source of PST.
		 * @note This is used mostly for determining the parent helios-scope of PST root elements.
		 */
		std::variant<MacroExpansionParent, ModuleParent> pst_parent;
	};

	/**
	 * @brief Base Element for all of the PST elements.
	 */
	class LangElement: public tpc::Element {
		THIS_CLASS(LangElement);

	protected:
		/**
		 * @TODO: #2938 Change position handling
		 * @TODO: #2938 Consider what should happen with context hash
		 * @TODO: #2938 Consider what should happen with token ownership
		 */
		explicit LangElement(pst::CloneDummy, const LangElement& other):
			  source_position(other.source_position),
			  context_hash(),
			  element_kind(other.element_kind),
			  id(PstID::next()) {}

		/**
		 * @brief Virtual function cloneElement that calls all of the parts of cloning that need to
		 * be done.
		 *
		 * The implementation is should only be defined in final elements.
		 */
		virtual CLONE_SIGNATURE() = 0;

		/**
		 * @brief A non-virtual function that is performed on each inheritance level of elements to
		 * clone the children elements.
		 */
		void cloneSubElements(const LangElement&) { return; }

	public:
		/**
		 * @brief Returns a clone of the PST subtree starting in the current element.
		 */
		[[nodiscard]]
		MBox<LangElement> clone() const;

		using SubToken = base::CRef<lexer::Token>;

		/**
		 * @brief Needed for access to element path methods.
		 */
		template<std::derived_from<LangElement>>
		friend class PST;

		/**
		 * @brief Needed for access to element path methods.
		 */
		friend class Stmt;

		template<typename X>
		friend class PSTAutomatic;
		friend class CloningUtils;

		using Child = AccessLocked<LangElement>;

		struct NamedChild final {
			std::string               name;
			AccessLocked<LangElement> element;
		};

		using SubElement = std::variant<SubToken, Child, NamedChild>;

		explicit LangElement(const LangParserState& state):
			  source_position(internal::getPosition(state)),
			  context_hash(internal::getContextHash(state)),
			  id(PstID::next()) {}

		LangElement(const LangElement&) = delete;
		LangElement(LangElement&&)      = delete;

		/**
		 * @brief Position covering the whole element.
		 */
		[[nodiscard]]
		SourcePositionLocked getSourcePosition() const;

		/**
		 * @brief The stable position of an element.
		 */
		[[nodiscard]]
		dia_int::StablePosition getStablePosition() const;

		/**
		 * @brief Given a StablePosition of an element, returns the source position of the element.
		 */
		static dia::SourcePosition getActiveSourcePosition(
			query::Context& ctx, const dia_int::StablePosition& pos
		);

		/**
		 * @brief Given a StablePosition of an element, returns the source position of the element,
		 * bypasses the query graph.
		 */
		static dia::SourcePosition getActiveSourcePositionIllegalAccess(
			const dia_int::StablePosition& pos
		);

		/**
		 * @brief Get pst node the by stable hash. Throws on non-existent hash.
		 * @note should not be used in query, currently used by by `queryPositionDependencies`
		 * machinery for test/insight purposes.
		 */
		[[nodiscard]]
		static AccessLocked<LangElement> getByStableHash(query::QueryStableHash stable_hash);

		/**
		 * @brief View all sub-elements.
		 */
		[[nodiscard]]
		auto viewSubElements() const {
			using namespace std::views;

			constexpr auto GET_LOCKED = [](const InternalSubElement& t) -> SubElement {
				if (base::holds<InternalChild>(t)) {
					return Child(std::get<InternalChild>(t));
				} else if (base::holds<InternalNamedChild>(t)) {
					auto& [name, inter] = std::get<InternalNamedChild>(t);
					return NamedChild{ .name = name, .element = { inter } };
				} else {
					return SubToken(std::get<SubToken>(t));
				}
			};

			return std::ranges::ref_view(sub_elements) | transform(GET_LOCKED);
		}

		/**
		 * @brief View all child elements.
		 */
		[[nodiscard]]
		auto viewChildren() const {
			using namespace std::views;

			constexpr auto IS_CHILD = [](const SubElement& t) -> bool {
				return base::holds<Child>(t) || base::holds<NamedChild>(t);
			};
			constexpr auto STRIP_NAME = [](const SubElement& t) -> const Child {
				if (base::holds<NamedChild>(t))
					return std::get<NamedChild>(t).element;
				else
					return std::get<Child>(t);
			};

			return viewSubElements() | filter(IS_CHILD) | transform(STRIP_NAME);
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

		[[nodiscard]]
		HashType getHash() const {
			CORE_ASSERT(hash.has_value(), "Hash not calculated for this" + elementType());
			return hash.value();
		}

		/**
		 * @return Whether an element is just a statement aggregate.
		 * As of 11.12.2024 there are 4 statement aggregates:
		 * * CodeBlock
		 * * CodeBlockOrStmt
		 * * TopLevel
		 * *
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
		const hashing::ComponentHash& getElementPathHash() const {
			CORE_ASSERT(
				element_path_hash.has_value(),
				"element path not calculated for this " + elementType()
			);
			return element_path_hash.value();
		}

		virtual void acceptVisitor(PstVisitor& visitor) const;

		[[nodiscard]]
		const AdditionalRootData& getAdditionalRootData() const {
			CORE_ASSERT(additional_root_data.has_value(), "Element has no additional root data");
			return additional_root_data.value();
		}

	protected:
		/**
		 * @brief Map from stable hash to lang element for all created elements.
		 * @note Used to view dependent tokens of node in the query graph and for HELIOS PST origin.
		 */
		static concurrent::ConHashMap<query::QueryStableHash, AccessLocked<LangElement>> pst_hash_map;

		using InternalChild = Ref<LangElement>;

		struct InternalNamedChild final {
			std::string      name;
			Ref<LangElement> element;
		};

		using InternalSubElement = std::variant<SubToken, InternalChild, InternalNamedChild>;

		/************************\
		|   LANG ELEMENT DATA    |
		\************************/

		dia::SourcePosition source_position;
		HashType            context_hash;

		/**
		 * All of the children elements meant for generic analysis of the tree.
		 */
		std::vector<InternalSubElement> sub_elements;

		/**
		 * Parent element in PST if element is not root.
		 */
		base::Optional<AccessLocked<LangElement>> parent;

		/**
		 * The Path that uniquely identifies the
		 * element and allows to conserve some
		 * information between compilations. Has
		 * no value if it's incalculable.
		 */
		base::Optional<hashing::ComponentHash> element_path_hash;

		/**
		 * The Hash that encodes the element path and data and allows to conserve some
		 * information between compilations. Has no value if it's incalculable.
		 */
		base::Optional<HashType> hash;

		// @TODO: #2404 We should have a RootElement that stores this information instead of storing
		// it in each element, but for now it is easier to keep it here.
		base::Optional<AdditionalRootData> additional_root_data;

		/**
		 * @brief Kind of the element.
		 * @note This is mostly for HELIOS to decide how to create scopes.
		 */
		ElementKind element_kind = ElementKind::KindNotSet;

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
		virtual void calcElementPathHashRecursive();

		/**
		 * @brief Calculates Element paths for this Element and children.
		 */
		void calcElementPathHash(const hashing::ComponentHash& path) {
			// append element type to incoming ComponentHash path
			element_path_hash = hashing::ComponentHash(path, elementType());
			calcElementPathHashRecursive();
		}

		/**
		 * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		 * from other elements (otherwise it would need each element would need to be a friend)
		 */
		template<typename Element>
		void calcChildPath(
			AccessInternalAnonymous<Element>& access_ref, const hashing::ComponentHash& path
		) const {
			if (auto ref = access_ref.internalMut()) ref->calcElementPathHash(path);
		}

		/**
		 * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		 * from other elements (otherwise it would need each element would need to be a friend)
		 */
		template<typename Element, base::TemplateStringLiteral name>
		void calcNamedChildPath(
			AccessInternal<Element, name>& access_ref, const hashing::ComponentHash& path
		) const {
			if (auto ref = access_ref.internalMut())
				ref->calcElementPathHash(hashing::ComponentHash(path, std::string_view(name.value)));
		}

		/**
		 * @brief Calculates Element paths for `access_ref` and children. A version that is visible
		 * from other elements (otherwise it would need each element would need to be a friend)
		 */
		template<typename Element>
		void calcIndexedListChildPath(
			std::span<AccessInternalAnonymous<Element>> vec, const hashing::ComponentHash& path
		) const {
			for (usize i = 0; i < vec.size(); i++) {
				hashing::ComponentHash child_path(path, std::format("[{}]", i));
				calcChildPath(vec[i], child_path);
			}
		}

		/**
		 * @brief Calculates Element paths for a completely ordered list of statements.
		 */
		void calcOrderedListChildPath(std::vector<AccessInternalAnonymous<Stmt>>&, const hashing::ComponentHash&);

		/**
		 * @brief Calculates the hashes recursively for the element and all children.
		 */
		void calcHashRecursive();

		/**
		 * @brief Calculates and sets the hash for this element, can be modified to change between
		 * stable and unstable hashes.
		 */
		void calcHash();

		/**
		 * @brief Puts the element in the global hash map, should be called at the end of hash
		 * calculation. Separated from `calcHash` to allow for different hash calculation strategies.
		 */
		void putInPSTHashHashMap();

		/**
		 * @brief Calls `putInPSTHashHashMap` recursively for this element and all children.
		 */
		void putInPSTHashHashMapRecursive();

		/**
		 * @brief Calculates the whole hash for the element including common parts like path and
		 * element type. Can be overriden for specific parent elements that add common information.
		 */
		[[nodiscard]]
		HashAlg calcStableHash() const;

		/**
		 * @brief Calculates the signature of the whole PST sub-tree. Assumes the hashes are already
		 * calculated.
		 * @param partial_hash partial hash to add the signature to
		 */
		void calcSignature(HashAlg& partial_hash) const;

		/**
		 * @brief Signs the hashes of the whole PST sub-tree with given signature.
		 */
		void signGenerated(const HashType& signature);

		/**
		 * @brief Used to add additional data that is generic to multiple elements for example in
		 * Stmt.
		 */
		virtual HashAlg& addGenericDataToHash(HashAlg& partial_hash) const;

		/**
		 * @brief Adds the element specific information to the hash (Not generic ones such as number
		 * of attributes or path). Should be overriden for each element.
		 * @important Each implementation has to return the same reference it received (similar to
		 * `<<` operator).
		 */
		virtual HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const = 0;

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

		/**
		 * @brief Sets the additional root data for this element.
		 * @note This should only be used for root elements of the PST.
		 */
		void setAdditionalRootData(AdditionalRootData data);

	private:
		PstID id = PstID::next();
	};

	using ImportType = CRef<pst::Import>;
}
