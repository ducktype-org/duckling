//
// Created by mateusz on 9/6/23.
//

#include "source_position.hpp"

#include <utility>

namespace lexer {
	std::string SourcePosition::getSourceChars() const {
		if (!sourceCode)
			return "Error: No source code given!";
		if (getStart() == -1 || getEnd() == -1)
			return "Error: Empty SourcePosition";

		std::string sourceChars = sourceCode->getContent().view().stdString();
		return sourceChars.substr(getStart(), getEnd() - getStart() + 1);
	}

	SourcePosition::SourcePosition() {
		lineNumber = columnNumber = sourceIndexStart = sourceIndexEnd = 0;
		this->sourceCode = nullptr;
	}

	SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> sourceCode): SourcePosition() {
		setSourceCode(std::move(sourceCode));
	}

	SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> sourceCode, u32 line, u32 column,
	                               u32 start)
		: SourcePosition(std::move(sourceCode)) {
		setLineNumber(line);
		setColumnNumber(column);
		setStart(start);
	}

	SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> sourceCode, u32 line, u32 column,
	                               u32 start, u32 end)
		: SourcePosition(std::move(sourceCode), line, column, start) {
		setEnd(end);
	}

	void SourcePosition::setLineNumber(u32 line) {
		lineNumber = line;
	}
	void SourcePosition::setColumnNumber(u32 column) {
		columnNumber = column;
	}
	void SourcePosition::setStart(u32 start) {
		sourceIndexStart = start;
		if (sourceIndexEnd < start)
			sourceIndexEnd = start;
	}

	void SourcePosition::setEnd(u32 end) {
		sourceIndexEnd = end;
		if (end < sourceIndexStart)
			sourceIndexEnd = sourceIndexStart;
	}

	void SourcePosition::setSourceCode(std::shared_ptr<fs::FilePath> newSourceCode) {
		sourceCode = std::move(newSourceCode);
	}

	usize SourcePosition::getStart() const {
		return sourceIndexStart;
	}
	usize SourcePosition::getEnd() const {
		return sourceIndexEnd;
	}
	usize SourcePosition::getLineNumber() const {
		return lineNumber;
	}
	usize SourcePosition::getColumn() const {
		return columnNumber;
	}
}
