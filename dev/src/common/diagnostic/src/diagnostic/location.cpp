#include "location.hpp"

#include <token_source/source.hpp>

namespace dia {
	void Location::printPrefixInfo(printer::PrinterOStream& out) const {
		out << "In file: " << getSourceFile().getFilePath().strView().data();
	}

	void Location::printMessage(
		printer::PrinterOStream&           out,
		const SourcePosition&              pos,
		const printer::PrinterContentsSeq& reason
	) const {
		// Printing position in the same line as file path in order to allow clicking on the path
		// in e.g. VSCode's terminal.
		printPrefixInfo(out);
		out << ":";
		pos.printPosition(out);
		out << "\nAt position ";
		pos.printPosition(out);
		out << ":\n" << reason << "\n";
		printPrettySourceLinesFromPosition(out, pos);
	}

	void Location::printSuffixInfo(printer::PrinterOStream&) const {}

	Ref<tokenizer::TokenSource> FileLocation::getSource() const { return source; }

	fs::File FileLocation::getSourceFile() const { return path; }

	MacroLocation::MacroLocation(const SourcePosition& parent, Ref<tokenizer::TokenSource> source):
		  parent(parent),
		  source(source),
		  path(parent.getSource()->getFile()) {}

	Ref<tokenizer::TokenSource> MacroLocation::getSource() const { return source; }

	fs::File MacroLocation::getSourceFile() const { return path; }

	void MacroLocation::printSuffixInfo(printer::PrinterOStream& out) const {
		out << "Expanded here: \n";
		parent.printPosition(out);
		printPrettySourceLinesFromPosition(out, parent);
	}

	void FakeLocation::printPrefixInfo(printer::PrinterOStream& out) const {
		out << "In an unspecified location: ";
	}

	void FakeLocation::printMessage(
		printer::PrinterOStream& out,
		const SourcePosition&,
		const printer::PrinterContentsSeq& reason
	) const {
		printPrefixInfo(out);
		out << reason << "\n";
	}

	Ref<tokenizer::TokenSource> FakeLocation::getSource() const { return source.refMut(); }

	fs::File FakeLocation::getSourceFile() const { return virtual_file; }

	Ref<FakeLocation> FakeLocation::getInstance() {
		static FakeLocation instance;
		return { &instance };
	}

	FakeLocation::FakeLocation():
		  virtual_file(fs::FileManager::createRandomVirtualFile("some example content here\n")),
		  source(tokenizer::makeTokenSource(virtual_file)) {
		auto previous_mode = lang_def::getKeywordMode();
		source->tokenize();
		lang_def::setKeywordMode(previous_mode);
	}
}
