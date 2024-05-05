/**
 * @file message.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "source_position.hpp"
#include "printer/message.hpp"
#include <cmath>
#include <format>
#include <string>
#include <token_file/file.hpp>
#include <base/exceptions.hpp>

namespace dia {
	std::vector<std::string> SourcePosition::getSourceLines() const {
		if (source_end == source_file->getChars().size() - 1) return {"<EOF>"};

		auto start = source_file->getLineColumn(source_start);
		auto end = source_file->getLineColumn(source_end);
		usize first_line = std::max((usize)2, start.first) - 1;
		usize last_line = std::min(source_file->getLines().size(), end.first + 1);
		usize length = std::to_string(last_line).size();
		std::string str_len = std::to_string(length);

		std::vector<std::string> res;
		res.emplace_back(std::string(length + 1, ' ') + "|\n");
		for(usize i = first_line; i <= last_line; i++) {
			res.emplace_back(std::vformat("{:>" + str_len + "} | ", std::make_format_args(i)));
			auto lineChars = source_file->getLine(i);
			auto line_begin = lineChars.front().raw_begin;
			auto line_end = lineChars.end()->raw_begin;
			res.emplace_back(reinterpret_cast<const char*>(line_begin), reinterpret_cast<const char*>(line_end));
			res.emplace_back("\n");
		}
		res.emplace_back(std::string(length + 1, ' ') + "|\n");

		return res;
	}

	SourcePosition::SourcePosition(
		tokenizer::File source_file,
		const usize       line,
		const usize       column,
		const usize       source_start
	):
		  SourcePosition(source_file, line, column, source_start, source_start) {}

	SourcePosition::SourcePosition(
		tokenizer::File source_file,
		const usize       line,
		const usize       column,
		const usize       source_start,
		const usize       source_end
	):
		  line(line),
		  column(column),
		  source_start(source_start),
		  source_end(source_end),
		  source_file(source_file) {
		// Potentially allow for special circumstances
		if (source_file == nullptr) throw base::LogicError("Invalid SourcePosition: No such file");
		if (line == 0) throw base::LogicError("Invalid SourcePosition: line = 0");
		if (column == 0) throw base::LogicError("Invalid SourcePosition: column = 0");
		if (source_end < source_start)
			throw base::LogicError("Invalid SourcePosition: source end before source start");
		// allow EOF position
		if (not(source_end == source_start and source_end == source_file->getChars().size() - 1)) {
			if (source_end >= source_file->getChars().size() - 1)
				throw base::LogicError("Invalid SourcePosition: source end outside the file");
		}
	}

	SourcePosition::SourcePosition(const SourcePosition& other, const usize source_end):
		  SourcePosition(
			  other.source_file, other.line, other.column, other.source_start, source_end
		  ) {}

	usize SourcePosition::getColumn() const { return column; }

	usize SourcePosition::getLine() const { return line; }

	usize SourcePosition::getStart() const { return source_start; }

	usize SourcePosition::getEnd() const { return source_end; }

	tokenizer::File SourcePosition::getSource() const { return source_file; }

	std::vector<printer::MessageContent>
		SourcePosition::genPrinterMessageContents(const printer::MessageContent& reason) const {
		std::vector<printer::MessageContent> res =  
				{ { "In file: " },
			     { source_file->getPath().strView().data() },
			     { ":" + std::to_string(line) + ":" + std::to_string(column) + "\n" },
			     reason,
			     { "\n" },
		};
		for (auto el: getSourceLines()) {
			res.emplace_back(std::move(el));
		}
		return res;
	}

	std::string SourcePosition::genStr(const std::string_view reason) const {
		std::string output = "In file: ";
		output += source_file->getPath().strView();
		output += ":" + std::to_string(line) + ":" + std::to_string(column) + "\n";
		output += reason;
		output += "\n";
		for(const auto& el: getSourceLines()) {
			output += el;
		}
		return output;
	}
}
