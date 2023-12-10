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
		UChar32 getValue() const;

		[[nodiscard]]
		std::string rawStr() const;

		[[nodiscard]]
		base::RawArray getRawBegin() const;		

		template<fs::Encoding encoding>
		friend CharArray decode(base::RawView bytes, dia::ErrorState&);

	};

	/**
	 * Decode array of bytes using given encoding
	 * 
	 * @tparam encoding Which encoding should function use
	 * @param bytes Vector of bytes to decode
	 * @param err ErrorState to store errors
	 * 
	 * @return CharArray of decoded data
	 * 
	 * @note We should probably stick to only decoding UTF-8 for now
	 */
	
	template<fs::Encoding encoding>
	CharArray decode(base::RawView bytes, dia::ErrorState& err);

	template<>
	CharArray decode<fs::US_ASCII>(base::RawView bytes, dia::ErrorState&);

	template<>
	CharArray decode<fs::UTF8>(base::RawView bytes, dia::ErrorState&);

}
