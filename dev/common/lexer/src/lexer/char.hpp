#pragma once

#include <filesystem/encoding.hpp>
#include <base/raw_view.hpp>
#include <vector>
#include <string>

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
		enum Type { Character, Digit, Operator, Whitespace, Special, Eof, Illegal, Empty };

		enum ParType { Round, Square, Curly, Angle, NotAPar };

		[[nodiscard]]
		bool isCharacter() const;
		[[nodiscard]]
		bool isDigit() const;
		[[nodiscard]]
		bool isOperator() const;
		[[nodiscard]]
		bool isSpecial() const;
		[[nodiscard]]
		bool isWhitespace() const;
		[[nodiscard]]
		bool isEOF() const;

		[[nodiscard]]
		bool isParOpen() const;
		[[nodiscard]]
		bool isParOpen(ParType type) const;
		[[nodiscard]]
		bool isParClose() const;
		[[nodiscard]]
		bool isParClose(ParType type) const;
		[[nodiscard]]
		ParType getParType() const;

		[[nodiscard]]
		bool isAscii() const;
		[[nodiscard]]
		char asciiValue() const;
		[[nodiscard]]
		bool isAsciiValue(char value) const;

		[[nodiscard]]
		Type getType() const;

		void appendRawValueTo(std::string& to) const;
		void appendRawValueTo(std::stringstream& to) const;

		[[nodiscard]]
		std::string rawStr() const;
		[[nodiscard]]
		std::string debugStr() const;

		template<fs::Encoding encoding>
		friend CharArray decode(base::RawView bytes);
		friend CharArray;

	private:
		Type           type_       = Empty;
		uchar          ascii_value = 0;
		base::RawArray raw_begin   = nullptr;

		// @TODO: this u8 is strange
		u8 size = u8{ 0 };

		Char() = default;

		// funkcje poniżej na wypadek, gdyby kiedyś więcej rzeczy się działo przy ustawianiu typu
		void setCharacter();
		void setDigit();
		void setOperator();
		void setSpecial();
	};

	/**
	 * @brief Class used to store the decoded file
	 * 
	 */
	class CharArray {
	public:
		/**
		 * @brief Type used to store the underlying data
		 * 
		 * @todo `r_array` member is unused. Delete it
		 */
		using Array = std::vector<Char>;
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
		base::RawView composeRaw(usize from, usize to) const;
		[[nodiscard]]
		base::RawView getRaw(usize i) const;
		[[nodiscard]]
		const Char& get(usize i) const;

		~CharArray();

	private:
		CharArray() = default;

		base::RawArray r_array = nullptr;
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
	 * @todo We should probably stick to only decoding UTF-8 for now
	 */
	template<fs::Encoding encoding>
	CharArray decode(base::RawView bytes);

	template<>
	CharArray decode<fs::US_ASCII>(base::RawView bytes);

	template<>
	CharArray decode<fs::UTF8>(base::RawView bytes);

}
