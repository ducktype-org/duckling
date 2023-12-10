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
	 * 
	 */
	class Char {
	public:
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

	private:
		UChar32          value = 0;
		base::RawArray raw_begin   = nullptr;
		u8 size = u8{ 0 };

		Char() = default;
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
