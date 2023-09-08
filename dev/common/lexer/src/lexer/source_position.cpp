//
// Created by mateusz on 9/6/23.
//

#include "source_position.hpp"

#include <utility>


std::string SourcePosition::getSourceChars() const {
	if(!sourceCode)
		return "Error: No source code given!";
	if(sourceIndexStart == -1 || sourceIndexEnd == -1)
		return "Error: Empty SourcePosition";

	std::string sourceChars = sourceCode->getContent().view().stdString();
	return sourceChars.substr(sourceIndexStart, sourceIndexEnd - sourceIndexStart + 1);
}

SourcePosition::SourcePosition() {
	sourceIndexStart = sourceIndexEnd = -1;
	this->sourceCode = nullptr;
}

SourcePosition::SourcePosition(SourcePosition& other) {
	sourceIndexStart = other.getPositions().first;
	sourceIndexEnd = other.getPositions().second;
	sourceCode = base::make_unique<fs::FilePath>(*other.sourceCode);
}

SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> sourceCode) : SourcePosition() {
	setSourceCode(std::move(sourceCode));
}

SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> sourceCode, u32 start) : SourcePosition(std::move(sourceCode)) {
    setStart(start);
}

SourcePosition::SourcePosition(std::shared_ptr<fs::FilePath> sourceCode, u32 start, u32 end) : SourcePosition(std::move(sourceCode), start) {
	setEnd(end);
}

void SourcePosition::setStart(u32 start) {
	sourceIndexStart = start;
	if(sourceIndexEnd < start)
		sourceIndexEnd = start;
}

void SourcePosition::setEnd(u32 end) {
	sourceIndexEnd = end;
	if(end < sourceIndexStart)
		sourceIndexEnd = sourceIndexStart;
}

std::pair<u32, u32> SourcePosition::getPositions() const {
	return std::make_pair(sourceIndexStart, sourceIndexEnd);
}
void SourcePosition::setSourceCode(std::shared_ptr<fs::FilePath> newSourceCode) {
	sourceCode = std::move(newSourceCode);
}

void swap(SourcePosition& first, SourcePosition& second) {
	using std::swap;
	swap(first.sourceIndexStart, second.sourceIndexStart);
	swap(first.sourceIndexEnd, second.sourceIndexEnd);
	swap(first.sourceCode, second.sourceCode);
}
