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

	Ref<tokenizer::TokenSource> FakeLocation::getSource() const {
		CORE_PANIC("Tried to access a TokenSource from fake location.");
	}

	fs::File FakeLocation::getSourceFile() const {
		CORE_PANIC("Tried to access a File from fake location.");
	}

	FakeLocation FakeLocation::instance = {};

	Ref<FakeLocation> FakeLocation::getInstance() { return { &instance }; }
}
