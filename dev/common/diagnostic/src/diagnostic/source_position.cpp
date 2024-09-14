/**
 * @file message.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include <string>
#include <printer/printer_content.hpp>
#include <token_file/file.hpp>
#include <base/exceptions.hpp>

#include "source_position.hpp"

namespace dia {
	std::vector<printer::PrinterContent> SourcePosition::getPrettySourceLines() const {
		usize start_line = getStartLineColumn().first;
		usize end_line   = getEndLineColumn().first;

		usize first_line = std::max((usize) 2, start_line) - 1;
		usize last_line  = std::min(source_file->getLines().size(), end_line + 1);

		usize begin_char = source_file->getLine(first_line).first;
		usize end_char   = source_file->getLine(last_line).second;

		usize       length  = std::to_string(last_line).size();
		std::string str_len = std::to_string(length);

		std::vector<printer::PrinterContent> res;
		res.emplace_back(std::string(length + 1, ' ') + "|");

		usize fixed_end = source_end;
		if (end_char == source_end) fixed_end--;

		auto before = source_file->viewSplitRange(begin_char, source_start);
		auto error  = source_file->viewSplitRange(source_start, fixed_end + 1);
		auto after  = source_file->viewSplitRange(fixed_end + 1, end_char);

		auto linePref = [&](usize line) {
			res.emplace_back("\n");
			std::stringstream number;
			number << std::setw((int) length) << line;
			res.emplace_back(number.str(), printer::Color::BRIGHT_BLUE);
			res.emplace_back(" | ");
		};
		usize prev_line = -1;

		for (auto [line, view]: before) {
			if (line != prev_line) {
				prev_line = line;
				linePref(line);
			}
			res.emplace_back(view.stdString());
		}
		for (auto [line, view]: error) {
			if (line != prev_line) {
				prev_line = line;
				linePref(line);
			}
			res.emplace_back(view.stdString(), printer::Color::BRIGHT_RED);
		}
		for (auto [line, view]: after) {
			if (line != prev_line) {
				prev_line = line;
				linePref(line);
			}
			res.emplace_back(view.stdString());
		}
		res.emplace_back("\n" + std::string(length + 1, ' ') + "|");

		return res;
	}

	SourcePosition::SourcePosition(tokenizer::BorrowFile source_file, const usize source_start):
		  SourcePosition(source_file, source_start, source_start) {}

	SourcePosition::SourcePosition(
		tokenizer::BorrowFile source_file, const usize source_start, const usize source_end
	):
		  source_start(source_start),
		  source_end(source_end),
		  source_file(source_file) {
		// Potentially allow for special circumstances
		if (source_file == nullptr) throw base::LogicError("Invalid SourcePosition: No such file");
		if (source_end < source_start)
			throw base::LogicError("Invalid SourcePosition: source end before source start");
		// allow EOF position
		if (not(source_end == source_start and source_end == source_file->getChars().size() - 1)) {
			if (source_end >= source_file->getChars().size() - 1)
				throw base::LogicError("Invalid SourcePosition: source end outside the file");
		}
	}

	SourcePosition::SourcePosition(const SourcePosition& other, const usize source_end):
		  SourcePosition(other.source_file, other.source_start, source_end) {}

	std::pair<usize, usize> SourcePosition::getStartLineColumn() const {
		return source_file.get() ? source_file->getLineColumn(source_start)
		                         : std::make_pair(usize(0), usize(0));
	}

	std::pair<usize, usize> SourcePosition::getEndLineColumn() const {
		return source_file.get() ? source_file->getLineColumn(source_end)
		                         : std::make_pair(usize(0), usize(0));
	}

	usize SourcePosition::getStart() const { return source_start; }

	usize SourcePosition::getEnd() const { return source_end; }

	tokenizer::BorrowFile SourcePosition::getSource() const { return source_file; }

	printer::PrinterContentsSeq
		SourcePosition::genPrinterContents(const printer::PrinterContent& reason) const {
		if (source_file == nullptr) {
			return {
				reason,
			};
		}
		auto [line, column]                      = getStartLineColumn();
		std::vector<printer::PrinterContent> res = {
			{ "In file: " }, { source_file->getPath().strView().data() },
			{ ":\n" },       { std::to_string(line), printer::Color::BRIGHT_BLUE },
			{ ":" },         { std::to_string(column), printer::Color::BRIGHT_BLUE },
			{ ": " },        reason,
			{ "\n" },
		};
		for (auto el: getPrettySourceLines()) res.push_back(std::move(el));
		return res;
	}

	std::string SourcePosition::genStr(const std::string_view reason) const {
		auto              content = genPrinterContents({ reason.data() });
		std::stringstream res;
		printer::StreamPrinter::printNL(content, res);
		return res.str();
	}

	void SourcePosition::semPrint(std::ostream& out) const {
		out << "\"position\": {";
		std::pair<usize, usize> start = this->getStartLineColumn();
		std::pair<usize, usize> end   = this->getEndLineColumn();
		out << "\"startLine\": " << start.first << ", ";
		out << "\"startColumn\": " << start.second << ", ";
		out << "\"endLine\": " << end.first << ", ";
		out << "\"endColumn\": " << end.second << ", ";
		out << "\"start\": " << this->source_start << ", ";
		out << "\"end\": " << this->source_end << "}";
	}
}
