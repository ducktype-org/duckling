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

SourcePosition::SourcePosition(const fs::FilePath& sourceCode) : SourcePosition() {
	this->sourceCode = std::make_unique<fs::FilePath>(sourceCode);
}

SourcePosition::SourcePosition(const fs::FilePath& sourceCode, u32 start) : SourcePosition(sourceCode) {
	sourceIndexStart = start;
}

SourcePosition::SourcePosition(const fs::FilePath& sourceCode, u32 start, u32 end) : SourcePosition(sourceCode, start) {
	sourceIndexEnd = end;
}

void SourcePosition::setStart(u32 start) {
	sourceIndexStart = start;
	if(sourceIndexEnd < start)
		sourceIndexEnd = start;
}

void SourcePosition::setEnd(u32 end) {
	sourceIndexEnd = end;
}

std::pair<u32, u32> SourcePosition::getPositions() const {
	return std::make_pair(sourceIndexStart, sourceIndexEnd);
}
SourcePosition::SourcePosition() {
	sourceIndexStart = sourceIndexEnd = -1;
}
void SourcePosition::setSourceCode(const fs::FilePath& newSourceCode) {
	sourceCode = std::make_unique<fs::FilePath>(newSourceCode);
}
