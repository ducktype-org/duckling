/**
 * @file message.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#include "source_position.hpp"

#include "location.hpp"

#include <base/except/exceptions.hpp>

#include <printer/printer_content.hpp>
#include <printer/stream_printer.hpp>
#include <token_source/source.hpp>

#include <algorithm>
#include <string>

namespace dia {
	void printLineNumber(printer::PrinterOStream& out, usize length, usize line, printer::Color col) {
		std::stringstream number;
		number << std::setw((int) length) << line;
		out.add({ number.str(), col });
		out << " | ";
	}

	void printPrettySourceLinesFromPosition(printer::PrinterOStream& out, const SourcePosition& pos) {
		auto source       = pos.getSource();
		auto source_start = pos.source_start;
		auto source_end   = pos.source_end;

		usize start_line = pos.getStartLineColumn().first;
		usize end_line   = pos.getEndLineColumn().first;

		usize first_line = std::max((usize) 2, start_line) - 1;
		usize last_line  = std::min(source->getLines().size(), end_line + 1);

		usize begin_char = source->getLine(first_line).first;
		usize end_char   = source->getLine(last_line).second;

		usize       length  = std::to_string(last_line).size();
		std::string str_len = std::to_string(length);

		out << std::string(length + 1, ' ') << "|";

		usize fixed_end = source_end;
		if (end_char == source_end) fixed_end--;

		auto before = source->viewSplitRange(begin_char, source_start);
		auto error  = source->viewSplitRange(source_start, fixed_end + 1);
		auto after  = source->viewSplitRange(fixed_end + 1, end_char);

		usize          prev_line = -1ULL;
		printer::Color line_col  = printer::Color::BrightBlue;

		for (auto [line, view]: before) {
			if (line != prev_line) {
				prev_line = line;
				out << "\n";
				printLineNumber(out, length, line, line_col);
			}
			out << view.stdString();
		}
		for (auto [line, view]: error) {
			if (line != prev_line) {
				prev_line = line;
				out << "\n";
				printLineNumber(out, length, line, line_col);
			}
			out.add({ view.stdString(), printer::Color::BrightRed });
		}
		for (auto [line, view]: after) {
			if (line != prev_line) {
				prev_line = line;
				out << "\n";
				printLineNumber(out, length, line, line_col);
			}
			out << view.stdString();
		}
		out << "\n" << std::string(length + 1, ' ') << "|";
	}

	SourcePosition::SourcePosition():
		  source_start(5),
		  source_end(11),
		  location_type(LocationType::FakeLocationType),
		  location(FakeLocation::getInstance()) {}

	SourcePosition::SourcePosition(CRef<Location> location, const usize source_start):
		  SourcePosition(location, source_start, source_start) {}

	SourcePosition::SourcePosition(
		CRef<Location> location, const usize source_start, const usize source_end
	):
		  source_start(source_start),
		  source_end(source_end),
		  location_type(location->getLocationType()),
		  location(location) {
		// Potentially allow for special circumstances
		auto source = location->getSource();
		if (source_end < source_start)
			throw base::LogicError("Invalid SourcePosition: source end before source start");
		// allow EOF position
		if (not(source_end == source_start and source_end == source->getChars().size() - 1)) {
			if (source_end >= source->getChars().size() - 1)
				throw base::LogicError("Invalid SourcePosition: source end outside the file");
		}
	}

	SourcePosition::SourcePosition(const SourcePosition& other, const usize source_end):
		  SourcePosition(other.location, other.source_start, source_end) {}

	SourcePosition SourcePosition::merge(const SourcePosition& lhs, const SourcePosition& rhs) {
		const auto* lhs_loc = &*lhs.location;
		const auto* rhs_loc = &*rhs.location;
		if (lhs_loc != rhs_loc) CORE_PANIC("Cannot merge SourcePositions from different locations");

		const auto merged_start = std::min(lhs.source_start, rhs.source_start);
		const auto merged_end   = std::max(lhs.source_end, rhs.source_end);
		return { lhs.location, merged_start, merged_end };
	}

	std::pair<usize, usize> SourcePosition::getStartLineColumn() const {
		return location->getSource()->getLineColumn(source_start);
	}

	std::pair<usize, usize> SourcePosition::getEndLineColumn() const {
		return location->getSource()->getLineColumn(source_end);
	}

	usize SourcePosition::getStart() const { return source_start; }

	usize SourcePosition::getEnd() const { return source_end; }

	Ref<tokenizer::TokenSource> SourcePosition::getSource() const { return location->getSource(); }

	CRef<Location> SourcePosition::getLocation() const { return location; }

	LocationType SourcePosition::getLocationType() const { return location_type; }

	void SourcePosition::printPosition(printer::PrinterOStream& out) const {
		auto [line, column] = getStartLineColumn();
		out << std::to_string(line) << ":" << std::to_string(column);
	}

	printer::PrinterContentsSeq SourcePosition::genPrinterContents(
		[[maybe_unused]] const printer::PrinterContentsSeq& reason
	) const {
		printer::PrinterOStream str;
		return str.getContents();
	}

	std::string SourcePosition::genStr(const std::string_view reason) const {
		auto              content = genPrinterContents({ reason.data() });
		std::stringstream res;
		printer::StreamPrinter::printNL(content, res);
		return res.str();
	}

	bool SourcePosition::isFileEnd() const {
		// EOF is always (last_char, last_char)
		return source_end == getSource()->getChars().size() - 1;
	}

	void SourcePosition::printToJson(std::ostream& out) const {
		out << "{";
		out << R"("sourceStart": )" << getStart() << ", ";
		out << R"("sourceEnd": )" << getEnd();
		out << "}";
	}

	std::strong_ordering SourcePosition::operator<=>(const dia::SourcePosition& other) const {
		auto loc_ord = &*getLocation() <=> &*other.getLocation();
		if (loc_ord != std::strong_ordering::equal) return loc_ord;

		auto start_ord = getStart() <=> other.getStart();
		if (start_ord != std::strong_ordering::equal) return start_ord;

		return getEnd() <=> other.getEnd();
	}

	bool SourcePosition::operator==(const SourcePosition& other) const {
		return getLocation() == other.getLocation() && getStart() == other.getStart()
		    && getEnd() == other.getEnd();
	}

	std::string SourcePosition::content() const {
		return getSource()->getCharRange(getStart(), getEnd() + 1).stdString();
	}
}
