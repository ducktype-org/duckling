// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/selector.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Comma separated selectors of a `using`/`import` statement: `a.b, c.*`.
	 *
	 * Not bracketed, ends on `;`, no trailing comma.
	 */
	class SelectorList final: public List<Selector, internal::NameGetters::selectorList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(SelectorList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit SelectorList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::SelectorList;
		}

		static MBox<SelectorList> parse(LangParserState& state);

		/**
		 * @brief Whether the whole list binds exactly one name, i.e. it is a single selector that
		 * `declaresSingleName()`.
		 *
		 * @note PST internal, it bypasses access locking.
		 */
		[[nodiscard]]
		bool bindsSingleName() const {
			return elements.size() == 1 && elements.front().internal()
			    && elements.front().internal()->declaresSingleName();
		}

		/**
		 * @brief The one name bound by the list, empty unless `bindsSingleName()`.
		 *
		 * @note PST internal, it bypasses access locking.
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getSingleDeclaredName() const {
			if (!bindsSingleName()) return {};
			return elements.front().internal()->getDeclaredName();
		}

		~SelectorList() final = default;
	};

	/**
	 * @brief Curly bracketed selectors after a dotted prefix: the `{c, d as e}` of
	 * `a.b.{c, d as e}`. A trailing comma is allowed.
	 */
	class NestedSelectorList final:
		  public List<Selector, internal::NameGetters::nestedSelectorList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(NestedSelectorList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit NestedSelectorList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::NestedSelectorList;
		}

		static MBox<NestedSelectorList> parse(LangParserState& state);

		~NestedSelectorList() final = default;
	};
}
