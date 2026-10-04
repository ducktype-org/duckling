#pragma once

#include "../meta.hpp"
#include "wrapper_elements/identifier_wrapper.hpp"

#include <base/pointers/default_deleter.hpp>

// `NestedSelectorList` stays incomplete here (it is a list of `Selector`s), so users of this
// header do not need `selector_list.hpp` just to destroy a `Selector`.
DEFAULT_BOX_PTR_DELETER_DECLARATION(pst::NestedSelectorList)

namespace pst {
	/**
	 * @brief What follows the dotted prefix of a `Selector`.
	 */
	enum class SelectorTail {
		None,    ///< `a.b.c`
		Star,    ///< `a.b.*` or `a.b.* hides x` or `a.b.* hides {x, y}`
		Nested,  ///< `a.b.{c, d as e}`
		As,      ///< `a.b.c as d`
	};

	/**
	 * @brief Selects names from a path, shared by `using` and `import`:
	 *
	 * ```
	 * Selector   ::= Ident ('.' Ident)* Tail?
	 * Tail       ::= '.*' ('hides' IdentGroup)?
	 *              | '.' '{' Selector (',' Selector)* ','? '}'
	 *              | 'as' Ident
	 * IdentGroup ::= Ident | '{' Ident (',' Ident)* ','? '}'
	 * ```
	 *
	 * The tail is a closed alternation, so `a.* as d` and `a.{b} as d` are parse errors.
	 */
	class Selector final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Selector, NotStmt, tail_kind);
		CLONE_SUBELEMENTS();

	private:
		std::vector<AccessInternalAnonymous<IdentifierWrapper>> names;
		SelectorTail                                            tail_kind = SelectorTail::None;
		NAMED_CHILD_OPT(as_name, IdentifierWrapper);
		NAMED_CHILD_OPT(nested, NestedSelectorList);
		std::vector<AccessInternalAnonymous<IdentifierWrapper>> hides;

	public:
		explicit Selector(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::Selector;
		}

		/**
		 * @brief Number of identifiers in the dotted prefix (`a.b.c` has 3).
		 */
		[[nodiscard]]
		usize numberOfNames() const {
			return names.size();
		}

		/**
		 * @brief Identifier of the dotted prefix at @p index.
		 */
		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getNameByIndex(usize index) const {
			return names[index].give();
		}

		/**
		 * @brief Which of the tail alternatives follows the prefix.
		 */
		[[nodiscard]]
		SelectorTail getTailKind() const {
			return tail_kind;
		}

		/**
		 * @brief Whether this is a `.*` selector (with or without `hides`).
		 */
		[[nodiscard]]
		bool isWildcard() const {
			return tail_kind == SelectorTail::Star;
		}

		/**
		 * @brief Whether this selector binds exactly one name: `a.b` or `a.b as c`.
		 */
		[[nodiscard]]
		bool declaresSingleName() const {
			return tail_kind == SelectorTail::None || tail_kind == SelectorTail::As;
		}

		/**
		 * @brief The name bound by the selector: the `as` name, else the last identifier.
		 *
		 * Empty unless `declaresSingleName()`.
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclaredName() const;

		/**
		 * @brief The identifier after `as`, set only for `SelectorTail::As`.
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getAsName() const {
			return as_name.map([](const auto& acc) { return acc.give(); });
		}

		/**
		 * @brief The `{ ... }` list, set only for `SelectorTail::Nested`.
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<NestedSelectorList>> getNested() const {
			return nested.map([](const auto& acc) { return acc.give(); });
		}

		/**
		 * @brief Number of names after `hides`, 0 when there is no `hides`.
		 */
		[[nodiscard]]
		usize numberOfHides() const {
			return hides.size();
		}

		/**
		 * @brief Name after `hides` at @p index.
		 */
		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getHideByIndex(usize index) const {
			return hides[index].give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Selector";
		}

		static MBox<Selector> parse(LangParserState& state);

		void dprint(std::ostream& out) const final;
		~Selector() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
		void     calcElementPathHashRecursive() override;
	};
}
