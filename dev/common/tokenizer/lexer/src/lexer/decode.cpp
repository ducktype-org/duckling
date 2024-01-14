#include "decode.hpp"
#include "classifications.hpp"

#include <base/convert.hpp>
#include <utility>

namespace lexer {
	template<>
	CharArray decode<fs::US_ASCII>(base::RawView bytes, dia::ErrorState& errorState) {
		std::vector<Char> out;
		for (usize i = 0; i < bytes.size(); i++) {
			// Check if valid ascii byte
			if ((bytes[i] & byte{ 0b10000000u }) != byte{ 0 }) {
				errorState.failAndLog({ { { "ASCII decoding error:", printer::Color::BRIGHT_RED },
				                          base::strConcat(
											  "undefined ASCII byte ",
											  base::toHexString(usize(bytes[i]), 2),
											  " encountered at position ",
											  i + 1
										  ) },
				                        printer::MessageType::ERROR });
				continue;
			}
			out.emplace_back(UChar32(bytes[i]), u8{ 1 }, bytes.getBegin() + i);
		}
		// Add eof value
		out.emplace_back(
			Classifications::end_of_file_value, u8{ 0 }, bytes.getBegin() + bytes.size()
		);
		return out;
	}

	template<>
	CharArray decode<fs::UTF8>(base::RawView bytes, dia::ErrorState& errorState) {
		std::vector<Char> out;

		auto log_error = [&](std::string message) {
			errorState.failAndLog({ { { "UTF-8 decoding error:", printer::Color::BRIGHT_RED },
			                          std::move(message) },
			                        printer::MessageType::ERROR });
		};

		usize pos = 0;
		while (pos < bytes.size()) {
			// Check if current byte is not a continuation byte
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
			// Figure out the size and value stored in the first byte
			usize size  = 1;
			auto  value = std::to_integer<UChar32>(bytes[pos]);
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

			// check continuation bytes for validity and figure out their value
			bool are_bytes_ok = true;
			for (usize new_pos = pos + 1; new_pos < pos + size && new_pos < bytes.size();
			     new_pos++) {
				if ((bytes[new_pos] & byte{ 0b11000000u }) != byte{ 0b10000000 }) {
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
					"EOF encountered before UTF-8 codepoint starting at byte ", pos + 1, " ended"
				));
				pos = bytes.size();
				continue;
			}

			// Check if value is a valid unicode code point
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

			out.emplace_back(value, u8(size), bytes.getBegin() + pos);
			pos += size;
		}
		// Add eof value
		out.emplace_back(
			Classifications::end_of_file_value, u8{ 0 }, bytes.getBegin() + bytes.size()
		);

		return out;
	}
}
