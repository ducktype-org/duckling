#include "utility.hpp"

#include <diagnostic/message.hpp>
#include <diagnostic/diagnostic_converters.hpp>
#include <printer/stream_printer.hpp>

namespace pst::internal {
	void printHighlight(dia::SourcePosition pos, const std::string& message) {
		auto note = makeBox<dia::PlaceholderMessage<dia::Hint, dia::Message::Domain::Parser>>(pos, message);
		auto content = dia::DiagnosticToUserConverter::toPrinterContents(note.ref(), true);
		printer::StreamPrinter::printNL(content, std::cerr);
	}
}
