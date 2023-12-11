#pragma once

#include <unicode/uniset.h>
#include <unicode/utypes.h>
#include <printer/printer.hpp>
#include <filesystem/encoding.hpp>
#include <error_state/error_state.hpp>
#include <base/raw_view.hpp>
#include <vector>
#include <string>
#include <optional>

namespace lexer {
	class Char;
	using CharArray = const std::vector<Char>;

	base::RawView composeRaw(CharArray&, usize begin, usize end);

	/**
	 * @brief Class used for characters and checking their classifications
	 */
	class Char {
	public:

		const UChar32 value;
		const u8 size;
		const base::RawArray raw_begin;

		Char(UChar32, u8, base::RawArray);

		[[nodiscard]]
		bool is(icu::UnicodeSet&) const;
		[[nodiscard]]
		bool is(UChar32) const;
		[[nodiscard]]
		bool isInRange(UChar32 begin, UChar32 end) const;

		[[nodiscard]]
		bool isBinDigit() const;
		[[nodiscard]]
		bool isDigit() const;
		[[nodiscard]]
		bool isHexDigit() const;

		/**
		 * @brief Returns value of paired bracket for brackets or value of this character otherwise
		 */
		[[nodiscard]]
		UChar32 bracketPair() const;

		[[nodiscard]]
		std::string rawStr() const;
	};
}
