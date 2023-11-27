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
	class CharArray;

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
		CharArray& operator=(CharArray&& other) noexcept;
		friend void swap(CharArray& first, CharArray& second);

		[[nodiscard]]
		const Array& getArray() const;

		[[nodiscard]]
		base::RawView composeRaw(usize from, usize to) const;

		[[nodiscard]]
		base::RawView getRaw(usize i) const;

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
