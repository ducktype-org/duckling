#include "location.hpp"

#include <lang_definitions/key_spec_op.hpp>
#include <token_source/source.hpp>

namespace dia {

	Ref<tokenizer::TokenSource> FileLocation::getSource() const { return source; }

	fs::File FileLocation::getSourceFile() const { return path; }

	MacroLocation::MacroLocation(
		const dia_int::StablePosition& parent, Ref<tokenizer::TokenSource> source
	):
		  parent(parent),
		  source(source),
		  path(parent.getActiveSourcePositionIllegalAccess().getSource()->getFile()) {}

	Ref<tokenizer::TokenSource> MacroLocation::getSource() const { return source; }

	fs::File MacroLocation::getSourceFile() const { return path; }

	dia_int::StablePosition MacroLocation::getMacroParentNode() const { return parent; }

	Ref<tokenizer::TokenSource> FakeLocation::getSource() const { return source.refMut(); }

	fs::File FakeLocation::getSourceFile() const { return virtual_file; }

	Ref<FakeLocation> FakeLocation::getInstance() {
		static FakeLocation instance;
		return { &instance };
	}

	FakeLocation::FakeLocation():
		  virtual_file(fs::FileManager::createRandomVirtualFile("some example content here\n")),
		  source(tokenizer::makeTokenSource(virtual_file)) {
		// This is a fix and is needed because the FakeLocation() can executed between the
		// tokenizing and parsing of some file with different keyword mode.
		auto previous_mode = lang_def::getKeywordMode();
		source->tokenize<lang_def::KeywordMode::DucklingSource>();
		lang_def::setKeywordMode(previous_mode);
	}
}
