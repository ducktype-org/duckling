/**
 * @file message.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include <printer/message.hpp>
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

		SourcePosition(const SourceFile& source_file, usize line, usize column, usize source_start);
		SourcePosition(
			const SourceFile& source_file,
			usize             line,
			usize             column,
			usize             source_start,
			usize             source_end
		);
		SourcePosition(const SourcePosition& other) = default;
		SourcePosition(const SourcePosition& other, usize source_end);

		SourcePosition& operator=(const SourcePosition& other) = default;

		/**
		 * @brief Get a copy of the bytes in this position
		 *
		 * @return std::string containing a copy of the bytes in this position.
		 */
		[[nodiscard]]
		std::string getSourceChars() const;

		/**
		 * @brief Get formatted message contents with a given reason.
		 *
		 * This is given as message contents (multiple) so that
		 * it can be conveniently decorated before wrapping in a printer::Message.
		 *
		 * @param reason The reason for the message.
		 * @return Formatted message contents.
		 */
		[[nodiscard]]
		std::vector<printer::MessageContent>
			genPrinterMessageContents(const printer::MessageContent& reason) const;

		/**
		 * @brief Get formatted message string with a given reason.
		 *
		 * @param reason The reason for the message.
		 * @return Formatted message string.
		 */
		[[nodiscard]]
		std::string genStr(std::string_view reason) const;

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

		// @TODO: this is a temporary solution:
		static SourcePosition getBadPosition() { return { nullptr, 0, 0, 0, 0 }; }

	private:
		usize line, column;      ///< #line, #column describe start position in code for the user
		usize source_start,
			source_end;          ///< #source_start, #source_end describe range of bytes in the file
		SourceFile source_file;  ///< pointer to source file data
	};
}
