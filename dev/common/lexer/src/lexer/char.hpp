#pragma once

#include <unicode/uniset.h>
#include <unicode/utypes.h>
#include <printer/printer.hpp>
#include <filesystem/encoding.hpp>
#include <base/raw_view.hpp>
#include <vector>
#include <string>
#include <optional>

namespace lexer {
	constexpr uchar bad_ascii = static_cast<uchar>(0b11111111);
	class CharArray;

	/**
	 * @brief Class used for characters and their classifications
	 * 
	 * @todo The current approach is a bit simplistic change it to reflect the requirements of handling unicode(It should probably keep codepoints in u32 or i32 and check for the characteristics that are interesting from the position of unicode.)
	 * @todo Divide Whitespace into EOL and vertical
	 * @todo Some characters might be in multiple `Type`-s
	 * @todo `ParType` being an enum might not be the best
	 */
	class Char {
	public:
		[[nodiscard]]
		bool is(icu::UnicodeSet&) const;
		[[nodiscard]]
		bool is(UChar) const;
		[[nodiscard]]
		bool isInRange(UChar begin, UChar end) const;

		[[nodiscard]]
		bool isBinDigit() const;
		[[nodiscard]]
		bool isDigit() const;
		[[nodiscard]]
		bool isHexDigit() const;

		[[nodiscard]]
		UChar bracketPair() const;

		template<fs::Encoding encoding>
		friend std::optional<CharArray> decode(base::RawView bytes, printer::Console&);
		friend CharArray;

	private:
		UChar32          value = 0;
		base::RawArray raw_begin   = nullptr;

		// @TODO: this u8 is strange
		u8 size = u8{ 0 };

		Char() = default;
	};

	/**
	 * @brief Class used to store the decoded file
	 * 
	 */
	class CharArray {
	public:
		/**
		 * @brief Type used to store the underlying data
		 */
		using Array = std::vector<Char>;
		CharArray() = default;
		CharArray(Array array);
		CharArray(CharArray&& other) noexcept;
		CharArray(const CharArray& other)      = delete;
		void operator=(const CharArray& other) = delete;
		[[nodiscard]]
		CharArray& operator=(CharArray&& other) noexcept;
		friend void swap(CharArray& first, CharArray& second);

		[[nodiscard]]
		const Array& getArray() const;

		[[nodiscard]]
		const Char& get(usize i) const;

	private:
		/**
		 * @brief Underlying data
		 */
		Array          array;
	};

	/**
	 * Decode array of bytes using given encoding
	 * 
	 * @tparam encoding Which encoding should function use
	 * @param bytes Vector of bytes to decode
	 * 
	 * @return CharArray of decoded data or std::nullopt if errors encountered
	 * 
	 * @note We should probably stick to only decoding UTF-8 for now
	 */
	
	template<fs::Encoding encoding>
	std::optional<CharArray> decode(base::RawView bytes, printer::Console&);

	template<>
	std::optional<CharArray> decode<fs::US_ASCII>(base::RawView bytes, printer::Console&);

	template<>
	std::optional<CharArray> decode<fs::UTF8>(base::RawView bytes, printer::Console&);

}
