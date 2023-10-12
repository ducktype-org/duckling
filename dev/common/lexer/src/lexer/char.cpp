/**
 * @file char.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "char.hpp"
#include <utility>
#include <vector>
#include <cctype>
#include <sstream>
#include <array>
#include <string>

namespace lexer {
	bool Char::isCharacter() const { return type_ == Character; }

	bool Char::isDigit() const { return type_ == Digit; }

	bool Char::isOperator() const { return type_ == Operator; }

	bool Char::isSpecial() const { return type_ == Special; }

	bool Char::isWhitespace() const { return type_ == Whitespace; }

	void Char::setCharacter() { type_ = Character; }

	void Char::setDigit() { type_ = Digit; }

	void Char::setOperator() { type_ = Operator; }

	void Char::setSpecial() { type_ = Special; }

	char Char::asciiValue() const {
		// This is theoretically unsafe:
		return static_cast<char>(ascii_value);
	}

	bool Char::isAsciiValue(char value) const { return asciiValue() == value; }

	std::string Char::rawStr() const {
		std::string out;
		out.reserve(uint8_t(size));
		for (usize i = 0; i < uint8_t(size); i++) out += (char) (*(raw_begin + i));
		return out;
	}

	CharArray::CharArray(Array array): array(std::move(array)) {}

	CharArray::CharArray(CharArray&& other) noexcept: CharArray() { swap(*this, other); }

	CharArray& CharArray::operator=(CharArray&& other) noexcept {
		swap(*this, other);
		return *this;
	}

	void swap(CharArray& first, CharArray& second) {
		using std::swap;

		swap(first.r_array, second.r_array);
		swap(first.array, second.array);
	}

	base::RawView CharArray::composeRaw(usize from, usize to) const {
		// for now assumes there are no illegal char
		base::RawArray begin = array[from].raw_begin;
		usize          size  = 0;
		for (usize i = from; i <= to; i++) {
			// @TODO: this is ugly, better solution should be made:
			size += usize{ uchar{ array[i].size } };
		}
		return { begin, size };
	}

	base::RawView CharArray::getRaw(usize i) const { return composeRaw(i, i); }

	const Char& CharArray::get(usize i) const { return array[i]; }

	const CharArray::Array& CharArray::getArray() const { return array; }

	CharArray::~CharArray() { delete[] r_array; }

	std::string Char::debugStr() const {
		std::stringstream out;
		switch (type_) {
		case Character:
			out << "Character";
			break;
		case Digit:
			out << "Digit";
			break;
		case Operator:
			out << "Operator";
			break;
		case Whitespace:
			out << "Whitespace";
			break;
		case Special:
			out << "Special";
			break;
		case Eof:
			out << "Eof";
			break;
		case Illegal:
			out << "Illegal";
			break;
		default:
			break;
		}
		out << ": `" << rawStr() << "`";
		return out.str();
	}

	Char::Type Char::getType() const { return type_; }

	void Char::appendRawValueTo(std::string& to) const {
		to += rawStr();  // todo .. optimize
	}

	void Char::appendRawValueTo(std::stringstream& to) const { to << asciiValue(); }

	bool Char::isEOF() const { return type_ == Eof; }

	bool Char::isParOpen() const {
		auto av = asciiValue();
		return av == '(' || av == '[' || av == '{';
	}

	bool Char::isParOpen(ParType type) const { return isParOpen() && getParType() == type; }

	bool Char::isParClose() const {
		auto av = asciiValue();
		return av == ')' || av == ']' || av == '}';
	}

	bool Char::isParClose(ParType type) const { return isParClose() && getParType() == type; }

	Char::ParType Char::getParType() const {
		switch (asciiValue()) {
		case '(':
		case ')':
			return Round;
		case '[':
		case ']':
			return Square;
		case '{':
		case '}':
			return Curly;
		default:
			return NotAPar;
		}
	}

	constexpr usize ASCII_LENGTH = 256;

	/**
	 * When modifing it modify also key_spec_op.cpp
	 */
	constexpr std::array<Char::Type, ASCII_LENGTH> makeCharTable() {
		std::array<Char::Type, ASCII_LENGTH> out = {};

		for (usize i = 0; i < ASCII_LENGTH; i++) out[i] = Char::Illegal;
		for (usize i = 'a'; i <= 'z'; i++) out[i] = Char::Character;
		for (usize i = 'A'; i <= 'Z'; i++) out[i] = Char::Character;
		for (usize i = '0'; i <= '9'; i++) out[i] = Char::Digit;
		out['_'] = Char::Character;

		// @TODO: this char is not perfect:
		constexpr uchar specials[] = R"--("@#$'();[\]`{})--";
		for (const auto& c: specials) out[c] = Char::Special;

		// @TODO: this char is not perfect:
		constexpr uchar operators[] = R"--(!%&*+-^|~:/.,<=>?)--";
		for (const auto& c: operators) out[c] = Char::Operator;

		out[' ']  = Char::Whitespace;
		out['\n'] = Char::Whitespace;
		out['\v'] = Char::Whitespace;
		out['\t'] = Char::Whitespace;

		return out;
	}

	constexpr std::array<Char::Type, 256> char_type_table = makeCharTable();

	template<fs::Encoding encoding>
	Char::Type charType(uchar ascii_value, base::RawArray r_data, usize size);

	Char::Type charType(uchar ascii_value) { return char_type_table[ascii_value]; }

	template<>
	Char::Type charType<fs::US_ASCII>(
		[[maybe_unused]] uchar ascii_value, base::RawArray r_data, [[maybe_unused]] usize size
	) {
		return charType((char) r_data[0]);
	}

	/**
	 * @TODO: detect unicode char types
	 * @TODO: make better parameters
	 */
	template<>
	Char::Type charType<fs::UTF8>(
		uint8_t ascii_value, [[maybe_unused]] base::RawArray r_data, [[maybe_unused]] usize size
	) {
		return (ascii_value == bad_ascii) ? Char::Character : charType(ascii_value);
	}

	template<>
	CharArray decode<fs::US_ASCII>(base::RawView bytes) {
		CharArray::Array out;
		for (usize i = 0; i < bytes.size(); i++) {
			Char next;
			next.ascii_value = uchar(bytes[i]);
			next.size        = u8(1);
			next.raw_begin   = bytes.getBegin() + i;
			next.type_       = charType(next.ascii_value);

			out.push_back(next);
		}
		return CharArray(std::move(out));
	}

	template<>
	CharArray decode<fs::UTF8>(base::RawView bytes) {
		CharArray::Array out;

		usize pos = 0;
		while (pos < bytes.size()) {
			while (((bytes[pos] ^ byte{ 0b10000000u }) & byte{ 0b11000000u }) == byte{ 0 }) {
				// bad char
				pos++;
			}
			usize size = 1;
			if ((bytes[pos] & byte{ 0b10000000u }) == byte{ 0 })
				size = 1;
			else if ((bytes[pos] & byte{ 0b00100000u }) == byte{ 0 })
				size = 2;
			else if ((bytes[pos] & byte{ 0b00010000u }) == byte{ 0 })
				size = 3;
			else if ((bytes[pos] & byte{ 0b00001000u }) == byte{ 0 })
				size = 4;

			Char next;
			next.ascii_value = ((bytes[pos] & byte{ 0b10000000u }) == byte{ 0 })
			                     ? static_cast<char>(bytes[pos])
			                     : bad_ascii;
			next.size        = u8(size);
			next.raw_begin   = bytes.getBegin() + pos;

			next.type_ = charType<fs::UTF8>(next.ascii_value, next.raw_begin, uchar{ next.size });

			out.push_back(next);
			pos += size;
		}

		return CharArray(std::move(out));
	}

}
