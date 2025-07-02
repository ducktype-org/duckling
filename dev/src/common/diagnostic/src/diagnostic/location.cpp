#include "location.hpp"

#include <token_source/source.hpp>

namespace dia {
	void Location::printPrefixInfo(printer::PrinterOStream& out) const {
		out << "In file: " << getSourceFile().strView().data();
	}

	void Location::printMessage(
		printer::PrinterOStream&           out,
		const SourcePosition&              pos,
		const printer::PrinterContentsSeq& reason
	) const {
		// e.g. VSCode's terminal.
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

	fs::FilePath FileLocation::getSourceFile() const { return path; }

	MacroLocation::MacroLocation(const SourcePosition& parent, Ref<tokenizer::TokenSource> source):
		  parent(parent),
		  source(source),
		  path(parent.getSource()->getPath()) {}

	Ref<tokenizer::TokenSource> MacroLocation::getSource() const { return source; }

	fs::FilePath MacroLocation::getSourceFile() const { return path; }

	void MacroLocation::printSuffixInfo(printer::PrinterOStream& out) const {
		out << "Expanded here: \n";
		parent.printPosition(out);
		printPrettySourceLinesFromPosition(out, parent);
	}

	void FakeLocation::printPrefixInfo(printer::PrinterOStream& out) const {
		out << "In and unspecified location: ";
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
		CORE_PANIC("Tried to access a fake location from a fake position.");
	}

	fs::FilePath FakeLocation::getSourceFile() const {
		CORE_PANIC("Tried to access a fake location from a fake position.");
	}

	FakeLocation FakeLocation::instance = {};

	Ref<FakeLocation> FakeLocation::getInstance() { return { &instance }; }
}
