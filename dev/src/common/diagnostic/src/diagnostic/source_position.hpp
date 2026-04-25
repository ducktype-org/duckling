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

#include "location_types.hpp"

#include <base/pointers/ref.hpp>

#include <printer/printer_content.hpp>
#include <printer/printer_ostream.hpp>
#include <token_source/forward.hpp>

#include <string>

namespace dia {
	class Location;
	class SourcePosition;

	/**
	 * @brief Print line number prefix.
	 *
	 * @param length - Number of characters to align to.
	 */
	void printLineNumber(printer::PrinterOStream&, usize length, usize line, printer::Color);

	/**
	 * @brief Get lines surrounding with position colored.
	 *
	 * @todo Maybe rewrite to use the general highlighted printing that is newer.
	 */
	void printPrettySourceLinesFromPosition(printer::PrinterOStream&, const SourcePosition&);

	/**
	 * @brief  Type used for storing token position in a source file
	 *
	 * It always stores a valid position with the position (EOF, EOF) being EOF
	 */
	class SourcePosition final {
	private:
		explicit SourcePosition();

		friend void printPrettySourceLinesFromPosition(printer::PrinterOStream&, const SourcePosition&);

	public:
		/**
		 * @brief Constructs a fake source position that should never be used except as an unused
		 * placeholder.
		 */
		static SourcePosition fakePosition() { return SourcePosition(); }

		/**
		 * @brief Merge two source ranges that refer to the same location into one enclosing range.
		 *
		 * @throws base::LogicError when locations differ.
		 */
		static SourcePosition merge(const SourcePosition& lhs, const SourcePosition& rhs);

		SourcePosition(CRef<Location>, usize source_start);
		SourcePosition(CRef<Location>, usize source_start, usize source_end);
		SourcePosition(const SourcePosition& other) = default;
		SourcePosition(const SourcePosition& other, usize source_end);

		SourcePosition& operator=(const SourcePosition& other) = default;

		bool operator==(const SourcePosition& other) const;

		/**
		 * @brief Order by tuple (filepath, source_start, source_end)
		 *
		 * @note Fine for now, In the future might break with macros as they share filepaths.
		 */
		std::strong_ordering operator<=>(const SourcePosition& other) const;

		/**
		 * @brief Get formatted message contents with a given reason.
		 *
		 * This is given as message contents (multiple) so that
		 * it can be conveniently decorated before wrapping in a printer::Message.
		 * This should be removed on the feature
		 * @param reason The reason for the message.
		 * @return Formatted message contents.
		 */
		[[nodiscard]]
		std::vector<printer::PrinterContent> genPrinterContents(
			[[maybe_unused]] const printer::PrinterContentsSeq& reason
		) const;

		/**
		 * @brief Get formatted message string with a given reason.
		 *
		 * @param reason The reason for the message.
		 * @return Formatted message string.
		 */
		[[nodiscard]]
		std::string genStr(std::string_view reason) const;

		void printPosition(printer::PrinterOStream&) const;

		[[nodiscard]]
		std::pair<usize, usize> getStartLineColumn() const;
		[[nodiscard]]
		std::pair<usize, usize> getEndLineColumn() const;
		[[nodiscard]]
		usize getStart() const;
		[[nodiscard]]
		usize getEnd() const;
		[[nodiscard]]
		Ref<tokenizer::TokenSource> getSource() const;
		[[nodiscard]]
		CRef<Location> getLocation() const;
		[[nodiscard]]
		LocationType getLocationType() const;

		/**
		 * @brief This checks exactly for position being EOF
		 */
		[[nodiscard]]
		bool isFileEnd() const;

		void printToJson(std::ostream&) const;

	private:
		usize          source_start;   ///< Start of the range of characters in the file.
		usize          source_end;     ///< End of the range of characters in the file.
		LocationType   location_type;  ///< Type of location the position is a part of.
		CRef<Location> location;       ///< Location of the position
	};
}
