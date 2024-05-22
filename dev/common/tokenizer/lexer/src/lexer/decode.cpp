#include "decode.hpp"
#include "base/borrow_pointer.hpp"
#include "classifications.hpp"
#include "diagnostic/source_position.hpp"
#include "token_file/forward.hpp"

#include <base/convert.hpp>
#include <token_file/file.hpp>
#include <printer/printer_content.hpp>

namespace lexer {

	/**
	 * @brief This is a bit of a corner case where positions don't make sense.
	 */
	class DecodingError final: public dia::Error {
	private:
		tokenizer::BorrowFile file;
		usize                 byte;
		std::string           reason;

	public:
		DecodingError() = delete;

		DecodingError(tokenizer::BorrowFile file, usize byte, std::string&& reason):
			  dia::Error(dia::SourcePosition::fakePosition()),
			  file(file),
			  byte(byte),
			  reason(std::move(reason)) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		[[nodiscard]]
		std::string toStringBrief() const override {
			std::stringstream res;
			res << "In file: " << file->getPath().strView() << "\nAt byte " << byte << ": "
				<< reason;
			return res.str();
		}
	};

	void logDecodeError(
		dia::Logger& log, tokenizer::BorrowFile file, usize byte, std::string&& reason
	) {
		log.log(base::make_unique<DecodingError>(file, byte, std::move(reason)));
	}

	template<>
	CharArray decode<fs::US_ASCII>(tokenizer::BorrowFile file, dia::Logger& log) {
		auto      bytes = file->getContent().view();
		CharArray out;
		for (usize i = 0; i < bytes.size(); i++) {
			// Check if valid ascii byte
			if ((bytes[i] & byte{ 0b10000000u }) != byte{ 0 }) {
				logDecodeError(
					log,
					file,
					i + 1,
					base::strConcat(
						"ASCII decoding error: ",
						"undefined ASCII byte ",
						base::toHexString(usize(bytes[i]), 2)
					)
				);
				continue;
			}
			out.emplace_back(UChar32(bytes[i]), u8{ 1 }, i);
		}
		// Add eof value
		out.emplace_back(Classifications::end_of_file_value, u8{ 0 }, bytes.size());
		return out;
	}

	template<>
	CharArray decode<fs::UTF8>(tokenizer::BorrowFile file, dia::Logger& log) {
		auto      bytes = file->getContent().view();
		CharArray out;

		auto log_error = [&](usize byte, const std::string& message) {
			logDecodeError(log, file, byte, "UTF-8 decoding error: " + message);
		};

		usize pos = 0;
		while (pos < bytes.size()) {
			// Check if current byte is not a continuation byte
			if (((bytes[pos] ^ byte{ 0b10000000u }) & byte{ 0b11000000u }) == byte{ 0 }) {
				log_error(
					pos + 1,
					base::strConcat(
						"Unexpected continuation byte ", base::toHexString(usize(bytes[pos]), 2)
					)
				);
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
				log_error(
					pos + 1,
					base::strConcat(
						"Invalid code-point starting byte ", base::toHexString(usize(bytes[pos]), 2)
					)
				);
				pos++;
				continue;
			}

			// check continuation bytes for validity and figure out their value
			bool are_bytes_ok = true;
			for (usize new_pos = pos + 1; new_pos < pos + size && new_pos < bytes.size();
			     new_pos++) {
				if ((bytes[new_pos] & byte{ 0b11000000u }) != byte{ 0b10000000 }) {
					are_bytes_ok = false;
					log_error(
						new_pos + 1,
						base::strConcat(
							"non-continuation byte ",
							base::toHexString(usize(bytes[new_pos]), 2),
							" where continuation from byte at position ",
							pos + 1,
							" was expected"
						)
					);
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
				log_error(
					bytes.size(),
					base::strConcat(
						"EOF encountered before UTF-8 codepoint starting at byte ",
						pos + 1,
						" ended"
					)
				);
				pos = bytes.size();
				continue;
			}

			// Check if value is a valid unicode code point
			if (!U_IS_UNICODE_CHAR(value)
			    || (U_GET_GC_MASK(value) & (U_GC_CN_MASK | U_GC_CO_MASK | U_GC_CS_MASK))) {
				log_error(
					pos + 1,
					base::strConcat(
						"codepoint undefined in the Unicode standard encountered starting here",
						" with value of ",
						base::toHexString(value)
					)
				);
				pos += size;
				continue;
			}

			out.emplace_back(value, u8(size), pos);
			pos += size;
		}
		// Add eof value
		out.emplace_back(Classifications::end_of_file_value, u8{ 0 }, bytes.size());

		return out;
	}
}
