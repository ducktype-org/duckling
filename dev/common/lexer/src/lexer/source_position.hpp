/**
 * @file source_position.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include <string>

namespace lexer {
	/** 
	 * @brief  Type used for storing token position in a source file
	 * 
	 * @todo add better error management
	 * @todo change it to immutable and check for errors on construction
	 */
	class SourcePosition {
	public:
		SourcePosition();

		explicit SourcePosition(std::shared_ptr<fs::FilePath> source_code);
		SourcePosition(std::shared_ptr<fs::FilePath> source_code, u64 line, u64 column, u64 start);
		SourcePosition(
			std::shared_ptr<fs::FilePath> source_code, u64 line, u64 column, u64 start, u64 end
		);

		/**
		 * @brief Get a copy of the bytes in this position
		 * 
		 * @return std::string containing a copy of the bytes in this position, if it fails it returns error message instead
		 * 
		 * @todo Change behaviour on error to be detectible
		 */
		[[nodiscard]]
		std::string getSourceChars() const;
		/**
		 * @brief generates formatted error message with a given reason
		 * 
		 * @param reason contains the reason for the error
		 * @return std::string containing the generated error message or, if it fails an error message
		 * 
		 * @todo Change behaviour on error to be detectible
		 */
		[[nodiscard]]
		std::string genErrorMsg(std::string_view reason) const;
		void        setSourceCode(std::shared_ptr<fs::FilePath> new_source_code);
		void        setLineNumber(usize line);
		void        setColumnNumber(usize column);
		void        setStart(usize start);
		void        setEnd(usize end);

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
		std::shared_ptr<fs::FilePath> source_code; ///< pointer to source file data
		usize line_number, column_number; ///< #line_number, #column_number describe start position in code for the user
		usize source_index_start, source_index_end;  ///< #source_index_start, #source_index_end describe range of bytes in the file
	};
}
