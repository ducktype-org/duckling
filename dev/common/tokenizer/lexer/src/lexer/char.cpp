/**
 * @file char.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "char.hpp"
#include "classifications.hpp"
#include <unicode/uchar.h>
#include <base/convert.hpp>
#include <base/str_concat.hpp>
#include <utility>
#include <vector>
#include <cctype>
#include <sstream>
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

	base::RawArray Char::getRawBegin() const {
		return raw_begin;
	}

	base::RawView composeRaw(CharArray& array, usize from, usize to) {
		base::RawArray begin = array.at(from).getRawBegin();
		usize size = array.at(to + 1).getRawBegin() - begin;
		return { begin, size };
	}

	template<>
	CharArray decode<fs::US_ASCII>(base::RawView bytes, dia::ErrorState& errorState) {
		std::vector<Char> out;
		for (usize i = 0; i < bytes.size(); i++) {
			if ((bytes[i] & byte{ 0b10000000u }) != byte{ 0 }) {
				errorState.failAndLog({{
								{"ASCII decoding error:", printer::Color::BRIGHT_RED},
								base::strConcat(
									"undefined ASCII byte ", 
									base::toHexString(usize(bytes[i]), 2), 
									" encountered at position ", 
									i + 1
								)},
							printer::MessageType::ERROR});
				continue;
			}
			Char next;
			next.value = UChar32(bytes[i]);
			next.size        = u8(1);
			next.raw_begin   = bytes.getBegin() + i;

			out.push_back(next);
		}
		Char eof;
		eof.value = Classifications::end_of_file_value;
		eof.size = u8(0);
		eof.raw_begin = bytes.getBegin() + bytes.size();
		out.push_back(eof);
		return CharArray(std::move(out));
	}

	template<>
	CharArray decode<fs::UTF8>(base::RawView bytes, dia::ErrorState& errorState) {
		std::vector<Char> out;

		auto log_error = [&](std::string message){
			errorState.failAndLog({{{"UTF-8 decoding error:", printer::Color::BRIGHT_RED},
				message}, printer::MessageType::ERROR});
		};

		usize pos = 0;
		while (pos < bytes.size()) {
			if (((bytes[pos] ^ byte{ 0b10000000u }) & byte{ 0b11000000u }) == byte{ 0 }) {
				log_error(base::strConcat(
					"Unexpected continuation byte ", 
					base::toHexString(usize(bytes[pos]), 2), 
					" encountered at byte position ", 
					pos + 1, 
					" during decoding"
				));
				pos++;
				continue;
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
			} else {
				log_error(base::strConcat(
					"Invalid code-point starting byte ",
					base::toHexString(usize(bytes[pos]), 2),
					" encountered at byte position ",
					pos + 1,
					" during decoding"
				));
				pos++;
				continue;
			}

			bool are_bytes_ok = true;
			for(usize new_pos = pos + 1; new_pos < pos + size && new_pos < bytes.size(); new_pos++) {
				if ((bytes[new_pos] & byte{ 0b11000000u }) != byte{ 0b10000000}) {
					are_bytes_ok = false;
					log_error(base::strConcat(
						"non-continuation byte ",
						base::toHexString(usize(bytes[new_pos]), 2),
						" encountered at byte position ", 
						new_pos + 1, 
						" where continuation from byte at position ", 
						pos + 1, 
						" was expected"
					));
					size = new_pos - pos;
					break;
				}
				value <<= 6;
				value += std::to_integer<UChar32>(bytes[new_pos]) & 0b00111111;
			}

			if (!are_bytes_ok) {
				pos += size;
				continue;
			}
			
			if (pos + size - 1 >= bytes.size()) {
				log_error(base::strConcat(
					"EOF encountered before UTF-8 codepoint starting at byte ", 
					pos + 1, 
					" ended"
				));
				pos = bytes.size();
				continue;
			}

			if (!U_IS_UNICODE_CHAR(value) 
				|| (U_GET_GC_MASK(value) & (U_GC_CN_MASK | U_GC_CO_MASK | U_GC_CS_MASK))) {
				log_error(base::strConcat(
					"codepoint undefined in the Unicode standard encountered starting at position ",
					pos + 1,
					" with value of ",
					base::toHexString(value)
				));
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
