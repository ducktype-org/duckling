/**
 * @file char.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "unicode/uchar.h"
#include "char.hpp"
#include "classifications.hpp"
#include <utility>
#include <vector>
#include <cctype>
#include <sstream>
#include <format>
#include <array>
#include <string>
#include <iostream>

namespace lexer {

	bool Char::is(icu::UnicodeSet& set) const {
		return set.contains(value);
	}

	bool Char::is(UChar32 c) const {
		return c == value;
	}

	bool Char::isInRange(UChar32 begin, UChar32 end) const {
		return value >= begin && value <= end;
	}

	bool Char::isBinDigit() const { 
		return is('0') or is('1'); 
	}

	bool Char::isDigit() const { return isInRange('0','9'); }

	bool Char::isHexDigit() const { 
		return isInRange('0','9') || isInRange('a', 'f') || isInRange('A', 'F'); 
	}

	UChar32 Char::getValue() const {
		return value;
	}

	UChar32 Char::bracketPair() const {
		return u_getBidiPairedBracket(value);
	}

	std::string Char::rawStr() const {
		std::string res;
		icu::UnicodeString(value).toUTF8String(res);
		return res;
	}

	CharArray::CharArray(Array array): array(std::move(array)) {}

	CharArray::CharArray(CharArray&& other) noexcept: CharArray() { swap(*this, other); }

	CharArray& CharArray::operator=(CharArray&& other) noexcept {
		swap(*this, other);
		return *this;
	}

	void swap(CharArray& first, CharArray& second) {
		using std::swap;

		swap(first.array, second.array);
	}

	const Char& CharArray::get(usize i) const { return array[i]; }

	const CharArray::Array& CharArray::getArray() const { return array; }

	base::RawView CharArray::composeRaw(usize from, usize to) const {
		base::RawArray begin = array[from].raw_begin;
		usize size = array[to + 1].raw_begin - begin;
		return { begin, size };
	}
	base::RawView CharArray::getRaw(usize i) const { return composeRaw(i, i); }

	template<>
	std::optional<CharArray> decode<fs::US_ASCII>(base::RawView bytes, printer::Console& console) {
		CharArray::Array out;
		bool is_error = false;
		for (usize i = 0; i < bytes.size(); i++) {
			if ((bytes[i] & byte{ 0b10000000u }) != byte{ 0 }) {
				is_error = true;
				console.add({{{"ASCII decoding error:", printer::Color::BRIGHT_RED},
							std::format("undefined ASCII byte {:#04X} encountered at position {}", std::to_integer<u16>(bytes[i]), i + 1)},
							printer::MessageType::ERROR});
				continue;
			}
			Char next;
			next.value = std::to_integer<UChar32>(bytes[i]);
			next.size        = u8(1);
			next.raw_begin   = bytes.getBegin() + i;

			out.push_back(next);
		}
		Char eof;
		eof.value = Classifications::end_of_file_value;
		eof.size = u8(0);
		eof.raw_begin = bytes.getBegin() + bytes.size();
		out.push_back(eof);
		if (is_error) return std::nullopt;
		return CharArray(std::move(out));
	}

	template<>
	std::optional<CharArray> decode<fs::UTF8>(base::RawView bytes, printer::Console& console) {
		CharArray::Array out;
		bool is_error = false;

		auto log_error = [&](std::string message){
			console.add({{{"UTF-8 decoding error:", printer::Color::BRIGHT_RED},
				message}, printer::MessageType::ERROR});
			is_error = true;
		};

		usize pos = 0;
		while (pos < bytes.size()) {
			while (((bytes[pos] ^ byte{ 0b10000000u }) & byte{ 0b11000000u }) == byte{ 0 }) {
				log_error(std::format("continuation byte {:#04X} encountered at byte position {} during decoding", std::to_integer<u16>(bytes[pos]), pos + 1));
				pos++;
			}
			usize size = 1;
			UChar32 value = std::to_integer<UChar32>(bytes[pos]);
			if ((bytes[pos] & byte{ 0b10000000u }) == byte{ 0 }) {
				size = 1;
				value &= 0b01111111;
			} else if ((bytes[pos] & byte{ 0b00100000u }) == byte{ 0 }) {
				size = 2;
				value &= 0b00011111;
			} else if ((bytes[pos] & byte{ 0b00010000u }) == byte{ 0 }) {
				size = 3;
				value &= 0b00001111;
			} else if ((bytes[pos] & byte{ 0b00001000u }) == byte{ 0 }) {
				size = 4;
				value &= 0b00000111;
			}

			bool are_bytes_ok = true;
			for(usize new_pos = pos + 1; new_pos < pos + size && new_pos < bytes.size(); new_pos++) {
				if ((bytes[new_pos] & byte{ 0b11000000u }) != byte{ 0b10000000}) {
					are_bytes_ok = false;
					log_error(std::format("non-continuation byte encountered at byte position {} where continuation from byte at position {} was expected", new_pos + 1, pos + 1));
					size = new_pos - pos;
					break;
				}
				value <<= 6;
				value += std::to_integer<UChar32>(bytes[new_pos]);
			}
			
			if (pos + size - 1 >= bytes.size()) {
				log_error(std::format("EOF encountered before UTF-8 codepoint at {} ended", pos + 1));
				pos = bytes.size();
				continue;
			}

			if (!are_bytes_ok) {
				pos += size;
				continue;
			}

			if (!U_IS_UNICODE_CHAR(value) 
				|| (U_GET_GC_MASK(value) & (U_GC_CN_MASK | U_GC_CO_MASK | U_GC_CS_MASK))) {
				log_error(std::format("codepoint undefined in the Unicode standard encountered starting at position {} with value of {:X}", pos + 1, value));
				pos += size;
				continue;
			}

			Char next;
			next.value = value;
			next.size        = u8(size);
			next.raw_begin   = bytes.getBegin() + pos;

			out.push_back(next);
			pos += size;
		}

		Char eof;
		eof.value = Classifications::end_of_file_value;
		eof.size = u8(0);
		eof.raw_begin = bytes.getBegin() + bytes.size();
		out.push_back(eof);

		return CharArray(std::move(out));
	}

}
