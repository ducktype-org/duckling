// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "access.hpp"
#include "elements/hierarchy/declarations/top_level.hpp"
#include "elements/includes/basic.hpp"  // IWYU pragma: keep

#include <time_stats/time_stats.hpp>

#include <diagnostic/logger.hpp>
#include <diagnostic/stable_position.hpp>
#include <token_source/source.hpp>

namespace pst {
	/**
	 * @brief PST generation class. Parses on construction if possible.
	 *
	 * @note The Element is only required to be derived from LangElement and not necessarily
	 * parsable to allow to manage already parsed generic PST<LangElement>.
	 *
	 * @tparam Element Root Element to parse.
	 */
	template<std::derived_from<LangElement> Element>
	class PST {
	protected:
		/****************\
		|    PST DATA    |
		\****************/

		/**
		 * Root element access wrapper for the parsed element tree.
		 */
		AccessInternalAnonymous<Element> element;

		/**
		 * Contextual component path/hash of this PST for hierarchical naming.
		 */
		hashing::ComponentHash hash_ctx_info;

		/***********************\
		|    PROTECTED METHODS    |
		\***********************/

		PST<>(hashing::ComponentHash&& hash_ctx_info): hash_ctx_info(std::move(hash_ctx_info)){};

		void assignRoot(MBox<Element>&& box) {
			CORE_ASSERT(!element.internal(), "Tried to overwrite root element.");
			element = std::move(box);
		}

		/**
		 * @brief Performs the element path calculation for all of the elements of the tree.
		 */
		void calcElementPathHash() {
			if (auto ref = element.internalMut()) ref->calcElementPathHash(hash_ctx_info);
		}

		/**
		 * @brief Performs the hash calculation for all of the elements of the tree.
		 */
		void calcHashes() {
			if (auto ref = element.internalMut()) ref->calcHashRecursive();
		}

		void putInPSTHashHashMap() {
			if (auto ref = element.internalMut()) ref->putInPSTHashHashMapRecursive();
		}

		/**
		 * @brief Calculates the total signature (Hash of the whole pst) and signs all of the
		 * elements with it (Adds it to their hash).
		 */
		void signGenerated() {
			if (auto ref = element.internalMut()) {
				HashAlg partial_hash{};
				ref->calcSignature(partial_hash);
				auto hash = partial_hash.finalize();
				ref->signGenerated(hash);
			}
		}

		/**
		 * @brief Does the additional work needed for generated PSTs
		 */
		void finishGeneratedPST() {
			calcElementPathHash();
			calcHashes();

			signGenerated();

			// we call it after signing, because signing changes the hash:
			putInPSTHashHashMap();
		}

		/**
		 * @brief Does the additional work needed for input PSTs
		 */
		void finishInputPST() {
			// Calculating paths should work for PSTs with parsing errors but if it becomes unstable
			// we might need to add an if for hasErrors.
			calcElementPathHash();
			calcHashes();

			putInPSTHashHashMap();
		}

	public:
		/**********************\
		|    PUBLIC METHODS    |
		\**********************/

		[[nodiscard]] virtual bool hasErrors() const = 0;

		[[nodiscard]]
		AccessLocked<Element> getRootElement() const {
			return element.give();
		}

		/**
		 * @TODO: #2397 Additional root data should just be passed during construction.
		 */
		void setAdditionalRootData(AdditionalRootData data) {
			CORE_ASSERT(
				element.internalMut().toOpt().has_value(),
				"Attempted to set additional root data on PST with null root element"
			);
			this->element.internalMut()->setAdditionalRootData(std::move(data));
		}

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }

		virtual ~PST() = default;
	};
}
