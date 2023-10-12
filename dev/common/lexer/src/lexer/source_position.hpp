/**
 * @file source_position.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include <string>

namespace lexer {
	// @TODO: Add class description and comments.
	class SourcePosition {
	public:
		SourcePosition();

		explicit SourcePosition(std::shared_ptr<fs::FilePath> source_code);
		SourcePosition(std::shared_ptr<fs::FilePath> source_code, u32 line, u32 column, u32 start);
		SourcePosition(
			std::shared_ptr<fs::FilePath> source_code, u32 line, u32 column, u32 start, u32 end
		);

		[[nodiscard]]
		std::string getSourceChars() const;
		[[nodiscard]]
		std::string genErrorMsg(std::string_view reason) const;
		void        setSourceCode(std::shared_ptr<fs::FilePath> new_source_code);
		void        setLineNumber(u32 line);
		void        setColumnNumber(u32 column);
		void        setStart(u32 start);
		void        setEnd(u32 end);

		[[nodiscard]]
		std::shared_ptr<fs::FilePath> getSourceCode();
		[[nodiscard]]
		usize getStart() const;
		[[nodiscard]]
		usize getEnd() const;
		[[nodiscard]]
		usize getLineNumber() const;
		[[nodiscard]]
		usize getColumn() const;

	private:
		std::shared_ptr<fs::FilePath> source_code;
		u32                           line_number, column_number, source_index_start,
			source_index_end;  // Indices of the characters in a source code.
	};
}
