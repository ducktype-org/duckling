/**
 * @file source_position.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include <printer/printer.hpp>
#include <string>

namespace dia {
	/**
	 * @brief  Type used for storing token position in a source file
	 *
	 * It always stores a valid position with the position (end() + 1, end() + 1) being EOF
	 */
	class SourcePosition {
	public:
		using SourceFile = std::shared_ptr<const fs::FilePath>;

		SourcePosition() = delete;

		SourcePosition(SourceFile source_file, usize line, usize column, usize start);
		SourcePosition(SourceFile source_file, usize line, usize column, usize start, usize end);
		SourcePosition(const SourcePosition& other) = default;
		SourcePosition(const SourcePosition& other, usize end);

		SourcePosition& operator=(const SourcePosition& other) = default;

		/**
		 * @brief Get a copy of the bytes in this position
		 *
		 * @return std::string containing a copy of the bytes in this position.
		 */
		[[nodiscard]]
		std::string getSourceChars() const;
		/**
		 * @brief generates formatted error message with a given reason
		 *
		 * @param reason contains the reason for the error
		 */
		[[nodiscard]]
		printer::Message genErrorMsg(std::string_view reason) const;
		/**
		 * @brief generates formatted error string with a given reason
		 *
		 * @param reason contains the reason for the error
		 */
		[[nodiscard]]
		std::string genErrorStr(std::string_view reason) const;

		[[nodiscard]]
		usize getLine() const;
		[[nodiscard]]
		usize getColumn() const;
		[[nodiscard]]
		usize getStart() const;
		[[nodiscard]]
		usize getEnd() const;
		[[nodiscard]]
		SourceFile getSource() const;

	private:
		usize line, column;      ///< #line, #column describe start position in code for the user
		usize source_start,
			source_end;          ///< #source_start, #source_end describe range of bytes in the file
		SourceFile source_file;  ///< pointer to source file data
	};
}
