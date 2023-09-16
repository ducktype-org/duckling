//
// Created by mateusz on 9/6/23.
//

#include "source_position.hpp"

#include <utility>

namespace lexer {
	std::string SourcePosition::getSourceChars() const {
		if (!source_code)
			return "Error: No source code given!";
		if (getStart() == -1 || getEnd() == -1)
			return "Error: Empty SourcePosition";

		std::string sourceChars = source_code->getContent().view().stdString();
		return sourceChars.substr(getStart(), getEnd() - getStart() + 1);
	}

	SourcePosition::SourcePosition() {
		line_number = column_number = source_index_start = source_index_end = 0;
		this->source_code = nullptr;
	}

	SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> source_code): SourcePosition() {
		setSourceCode(std::move(source_code));
	}

	SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> source_code, u32 line, u32 column,
	                               u32 start)
		: SourcePosition(std::move(source_code)) {
		setLineNumber(line);
		setColumnNumber(column);
		setStart(start);
	}

	SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> source_code, u32 line, u32 column,
	                               u32 start, u32 end)
		: SourcePosition(std::move(source_code), line, column, start) {
		setEnd(end);
	}

	void SourcePosition::setLineNumber(u32 line) {
		line_number = line;
	}
	void SourcePosition::setColumnNumber(u32 column) {
		column_number = column;
	}
	void SourcePosition::setStart(u32 start) {
		source_index_start = start;
		if (source_index_end < start)
			source_index_end = start;
	}

	void SourcePosition::setEnd(u32 end) {
		source_index_end = end;
		if (end < source_index_start)
			source_index_end = source_index_start;
	}

	void SourcePosition::setSourceCode(std::shared_ptr<fs::FilePath> new_source_code) {
		source_code = std::move(new_source_code);
	}

	usize SourcePosition::getStart() const {
		return source_index_start;
	}
	usize SourcePosition::getEnd() const {
		return source_index_end;
	}
	usize SourcePosition::getLineNumber() const {
		return line_number;
	}
	usize SourcePosition::getColumn() const {
		return column_number;
	}
	std::string SourcePosition::genErrorMsg(std::string_view reason) const {
		if(line_number == 0)
			return "Error getting info: SourcePosition is invalid";

		std::string output = "In file: ";
		output += source_code->strView();
		output += ":" + std::to_string(line_number) + ":" + std::to_string(column_number) + "\n";
		output += "error: ";
		output += reason;
		output += "\n";
		output += getSourceChars() + "\n";
		return output;
	}
}
