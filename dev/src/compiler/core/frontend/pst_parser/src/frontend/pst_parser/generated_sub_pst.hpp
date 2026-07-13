#pragma once

#include "pst.hpp"

#include "elements/hierarchy/not_statements/synthetic_elements/template_top_level.hpp"

namespace pst {
	/**
	 * @brief PST holder for generated sub trees. For now will be used for template expansion.
	 *
	 * @TODO: Maybe add some information about the original PST.
	 */
	template<std::derived_from<LangElement> Element = TemplateTopLevel>
	class GeneratedSubPST final {
	private:
		/****************\
		|    PST DATA    |
		\****************/

		// Not needed as the token source is in the original pst
		// Box<tokenizer::TokenSource> file;

		/**
		 * Root element access wrapper for the parsed element tree.
		 */
		AccessInternalAnonymous<Element> element;

		// Not needed as we are not doing whole file templates for now. They might also work differently anyway.
		// std::vector<ImportType> imports;

		/**
		 * Contextual component path/hash of this PST for hierarchical naming.
		 */
		hashing::ComponentHash hash_ctx_info;

		/***********************\
		|    PRIVATE METHODS    |
		\***********************/

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

		GeneratedSubPST(Box<Element>&& el, hashing::ComponentHash&& hash_ctx): element(std::move(el)), hash_ctx_info(std::move(hash_ctx)) {};

	public:
		/**********************\
		|    PUBLIC METHODS    |
		\**********************/

		static GeneratedSubPST fromTree(
			Box<Element>&& el,
			hashing::ComponentHash&&  hash_ctx = {}
		) {
			auto out = GeneratedSubPST(std::move(el), std::move(hash_ctx));

			out.calcElementPathHash();
			out.calcHashes();

			out.signGenerated();

			// we call it again after signing, because signing changes the hash:
			out.putInPSTHashHashMap();
			return out;
		}

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
	};
}
