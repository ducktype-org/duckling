#include "location.hpp"

#include <token_file/file.hpp>

namespace dia {

	fs::FilePath Location::getSourceFile() const { return getSource()->getPath(); }

	void Location::printPrefixInfo(printer::PrinterOStream& out) const {
		out << "In file: " << getSourceFile().strView().data() << "\n";
	}

	void Location::printMessage(
		printer::PrinterOStream&           out,
		const SourcePosition&              pos,
		const printer::PrinterContentsSeq& reason
	) const {
		printPrefixInfo(out);
		out << "At position ";
		pos.printPosition(out);
		out << ":\n" << reason << "\n";
		printPrettySourceLinesFromPosition(out, pos);
	}

	void Location::printSuffixInfo(printer::PrinterOStream&) const {}

	Ref<tokenizer::TokenFile> FileLocation::getSource() const { return file; }

	MacroLocation::MacroLocation(const SourcePosition& parent):
		  parent(parent),
		  file(parent.getSource()) {}

	Ref<tokenizer::TokenFile> MacroLocation::getSource() const { return file; }

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

	Ref<tokenizer::TokenFile> FakeLocation::getSource() const {
		CORE_PANIC("Tried to access a fake location from a fake position.");
	}

	FakeLocation FakeLocation::instance = {};

	Ref<FakeLocation> FakeLocation::getInstance() { return { &instance }; }
}
