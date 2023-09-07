//
// Created by mateusz on 9/6/23.
//

#ifndef RIFT_COMMON_LEXER_SRC_LEXER_SOURCE_POSITION_HPP_
#define RIFT_COMMON_LEXER_SRC_LEXER_SOURCE_POSITION_HPP_

#include <filesystem/file.hpp>
#include <string>

// @TODO: Add class description and comments.
class SourcePosition {
public:
	SourcePosition();
	SourcePosition(SourcePosition&);
	SourcePosition(SourcePosition&&) noexcept;

	explicit SourcePosition(const fs::FilePath& sourceCode);
	SourcePosition(const fs::FilePath& sourceCode, u32 start);
	SourcePosition(const fs::FilePath& sourceCode, u32 start, u32 end);

	friend void swap(SourcePosition& first, SourcePosition& second);
	[[nodiscard]] std::string getSourceChars() const;
	void setSourceCode(const fs::FilePath & newSourceCode);
	void setStart(u32 start);
	void setEnd(u32 end);
	[[nodiscard]] std::pair<u32, u32> getPositions() const;
private:
	base::unique_ptr<fs::FilePath> sourceCode;
	u32 sourceIndexStart, sourceIndexEnd; // Indices of the characters in a source code.
};


#endif // RIFT_COMMON_LEXER_SRC_LEXER_SOURCE_POSITION_HPP_
