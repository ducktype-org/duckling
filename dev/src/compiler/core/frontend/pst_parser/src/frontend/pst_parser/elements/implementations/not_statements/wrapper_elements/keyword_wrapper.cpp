// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/wrapper_elements/keyword_wrapper.hpp"

#include "../preamble.hpp"

namespace pst {

	MBox<KeywordWrapper> KeywordWrapper::parse(LangParserState& state) {
		// For now we use binary operator as it's the least restrictive.
		auto key = state[0].asKeyword();

		if (key == lang_def::Keyword::NotAKeyword) {
			state.logInt(base::makeBox<tpc::NoKeywordError>(
				state.getPosition(), state.ctokens().peek().describe()
			));
		}

		Box<KeywordWrapper> out = makeBox<KeywordWrapper>(state, key);

		PARSE().one(key);

		PST_RETURN out;
	}

	HashAlg& KeywordWrapper::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, key);
		return partial_hash;
	}

	void KeywordWrapper::dprint(std::ostream& out) const {
		out << "{";
		out << "\"value\" : ";
		tpc::nullAwareDprint(key, out);
		out << "}";
	}
}
