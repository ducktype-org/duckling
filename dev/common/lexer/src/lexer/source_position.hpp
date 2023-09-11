//
// Created by mateusz on 9/6/23.
//

#ifndef RIFT_COMMON_LEXER_SRC_LEXER_SOURCE_POSITION_HPP_
#define RIFT_COMMON_LEXER_SRC_LEXER_SOURCE_POSITION_HPP_

#include <filesystem/file.hpp>
#include <string>

namespace lexer {
	// @TODO: Add class description and comments.
	class SourcePosition {
	public:
		SourcePosition();

		explicit SourcePosition(std::shared_ptr<fs::FilePath> sourceCode);
		SourcePosition(std::shared_ptr<fs::FilePath> sourceCode, u32 line, u32 column, u32 start);
		SourcePosition(std::shared_ptr<fs::FilePath> sourceCode, u32 line, u32 column, u32 start,
		               u32 end);

		[[nodiscard]] std::string getSourceChars() const;
		void setSourceCode(std::shared_ptr<fs::FilePath> newSourceCode);
		void setLineNumber(u32 line);
		void setColumnNumber(u32 column);
		void setStart(u32 start);
		void setEnd(u32 end);

		[[nodiscard]] usize getStart() const;
		[[nodiscard]] usize getEnd() const;
		[[nodiscard]] usize getLineNumber() const;
		[[nodiscard]] usize getColumn() const;

	private:
		std::shared_ptr<fs::FilePath> sourceCode;
		u32 lineNumber, columnNumber, sourceIndexStart,
			sourceIndexEnd; // Indices of the characters in a source code.
	};
}


#endif // RIFT_COMMON_LEXER_SRC_LEXER_SOURCE_POSITION_HPP_
