/**
 * @file source_position.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief Module implementing source position handling.
 *
 * ### Usage:
 * @code
int main() {
    dia::SourcePosition singleCharacter{<source-file>, <line>, <column>, <position-in-file>};
    dia::SourcePosition multipleCharacters{<source-file>, <line>, <column>, <start-position>,
<end-position>}; dia::SourcePosition rangeFromSingleCharacter{singleCharacter, <end-position>};

    std::cerr << multipleCharacters.genErrorStr("some message") << "\n";
}
 @endcode
 */

#pragma once

#include <token_file/forward.hpp>
#include <filesystem/file.hpp>
#include <memory>
#include <printer/printer_content.hpp>
#include <string>

namespace dia {
	/**
	 * @brief  Type used for storing token position in a source file
	 *
	 * It always stores a valid position with the position (EOF, EOF) being EOF
	 */
	class SourcePosition final {
	private:
		explicit SourcePosition(): source_start(0), source_end(0), source_file(nullptr) {}

	public:
		/**
		 * @brief Constructs a fake source position that should never be used except as an unused
		 * placeholder.
		 */
		static SourcePosition fakePosition() { return SourcePosition(); }

		SourcePosition(tokenizer::BorrowFile source_file, usize source_start);
		SourcePosition(tokenizer::BorrowFile source_file, usize source_start, usize source_end);
		SourcePosition(const SourcePosition& other) = default;
		SourcePosition(const SourcePosition& other, usize source_end);

		SourcePosition& operator=(const SourcePosition& other) = default;

		/**
		 * @brief Get lines surrounding with error colored.
		 */
		[[nodiscard]]
		std::vector<printer::PrinterContent> getPrettySourceLines() const;

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
		std::vector<printer::PrinterContent>
			genPrinterContents(const printer::PrinterContent& reason) const;

		/**
		 * @brief Get formatted message string with a given reason.
		 *
		 * @param reason The reason for the message.
		 * @return Formatted message string.
		 */
		[[nodiscard]]
		std::string genStr(std::string_view reason) const;

		[[nodiscard]]
		std::pair<usize, usize> getStartLineColumn() const;
		[[nodiscard]]
		std::pair<usize, usize> getEndLineColumn() const;
		[[nodiscard]]
		usize getStart() const;
		[[nodiscard]]
		usize getEnd() const;
		[[nodiscard]]
		tokenizer::BorrowFile getSource() const;

	private:
		usize                 source_start;  ///< Start of the range of characters in the file.
		usize                 source_end;    ///< End of the range of characters in the file.
		tokenizer::BorrowFile source_file;   ///< Pointer to source file data.
	};
}
