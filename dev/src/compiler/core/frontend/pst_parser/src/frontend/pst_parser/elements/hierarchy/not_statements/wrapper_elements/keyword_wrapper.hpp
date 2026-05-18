#pragma once

#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Used for elements which can have different keywords like class specials.
	 */
	class KeywordWrapper final: public NotStmt {
		lang_def::Keyword key;

	public:
		explicit KeywordWrapper(LangParserState& state, lang_def::Keyword key):
			  NotStmt(state),
			  key(key) {
			element_kind = ElementKind::KeywordWrapper;
		}

		static MBox<KeywordWrapper> parse(LangParserState& state);
		~KeywordWrapper() final = default;

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		lang_def::Keyword unwrap() const {
			return key;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Keyword Wrapper";
		}
	};
}
