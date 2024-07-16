#include "decode.hpp"
#include "classifications.hpp"

#include <base/borrow_pointer.hpp>
#include <diagnostic/source_position.hpp>
#include <token_file/forward.hpp>
#include <base/convert.hpp>
#include <token_file/file.hpp>

namespace lexer {

	namespace detail {
		std::string decodeError(tokenizer::BorrowFile file, usize byte, const std::string& reason) {
			std::stringstream res;
			res << "In file: " << file->getPath().strView() << "\nAt byte " << byte << ": "
				<< reason;
			return res.str();
		}
	}

	/**
	 * @note This is a bit of a corner case where positions don't make sense.
	 */
	class DecodingError: public dia::Error {
	private:
		tokenizer::BorrowFile file;
		usize                 byte;

	public:
		DecodingError(tokenizer::BorrowFile file, usize byte):
			  dia::Error(dia::SourcePosition::fakePosition()),
			  file(std::move(file)),
			  byte(byte) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		[[nodiscard]]
		std::string toStringBrief() const override {
			return detail::decodeError(file, byte, reason());
		}

		[[nodiscard]]
		virtual std::string reason() const
			= 0;
	};

	class AsciiByteError final: public DecodingError {
	private:
		usize bad_byte;

	public:
		AsciiByteError(tokenizer::BorrowFile file, usize byte, usize bad_byte):
			  DecodingError(file, byte),
			  bad_byte(bad_byte) {}

		[[nodiscard]]
		std::string reason() const override {
			return "ASCII decoding error: undefined ASCII byte " + base::toHexString(bad_byte, 2);
		}
	};

	class Utf8UnexpectedContinuationError final: public DecodingError {
	private:
		usize bad_byte;

	public:
		Utf8UnexpectedContinuationError(tokenizer::BorrowFile file, usize byte, usize bad_byte):
			  DecodingError(file, byte),
			  bad_byte(bad_byte) {}

		[[nodiscard]]
		std::string reason() const override {
			return "UTF-8 decoding error: Unexpected continuation byte "
			     + base::toHexString(bad_byte, 2);
		}
	};

	class Utf8BadByteStartError final: public DecodingError {
	private:
		usize bad_byte;

	public:
		Utf8BadByteStartError(tokenizer::BorrowFile file, usize byte, usize bad_byte):
			  DecodingError(file, byte),
			  bad_byte(bad_byte) {}

		[[nodiscard]]
		std::string reason() const override {
			return "UTF-8 decoding error: Invalid code-point starting byte "
			     + base::toHexString(bad_byte, 2);
		}
	};

	class Utf8BadNonContinuationError final: public DecodingError {
	private:
		usize bad_byte;
		usize code_point_start;

	public:
		Utf8BadNonContinuationError(
			tokenizer::BorrowFile file, usize byte, usize bad_byte, usize code_point_start
		):
			  DecodingError(file, byte),
			  bad_byte(bad_byte),
			  code_point_start(code_point_start) {}

		[[nodiscard]]
		std::string reason() const override {
			std::stringstream ss;
			ss << "UTF-8 decoding error: non-continuation byte ";
			ss << base::toHexString(bad_byte, 2);
			ss << " where continuation byte from code-point starting at position ";
			ss << code_point_start << " was expected.";
			return ss.str();
		}
	};

	class Utf8BadEofError final: public DecodingError {
	private:
		usize code_point_start;

	public:
		Utf8BadEofError(tokenizer::BorrowFile file, usize byte, usize code_point_start):
			  DecodingError(file, byte),
			  code_point_start(code_point_start) {}

		[[nodiscard]]
		std::string reason() const override {
			std::stringstream ss;
			ss << "UTF-8 decoding error: EOF encountered before UTF-8 codepoint starting at byte ";
			ss << code_point_start << " ended.";
			return ss.str();
		}
	};

	class Utf8UndefinedCodepointError final: public DecodingError {
	private:
		UChar32 value;

	public:
		Utf8UndefinedCodepointError(tokenizer::BorrowFile file, usize byte, UChar32 value):
			  DecodingError(file, byte),
			  value(value) {}

		[[nodiscard]]
		std::string reason() const override {
			return base::strConcat(
				"UTF-8 decoding error: ",
				"Codepoint undefined in the Unicode standard encountered starting here ",
				"with value of ",
				base::toHexString(value)
			);
		}
	};

	template<>
	CharArray decode<fs::US_ASCII>(tokenizer::BorrowFile file, dia::Logger& log) {
		auto      bytes = file->getContent().view();
		CharArray out;
		for (usize i = 0; i < bytes.size(); i++) {
			// Check if valid ascii byte
			if ((bytes[i] & byte{ 0b10000000u }) != byte{ 0 }) {
				log.log(base::make_unique<AsciiByteError>(file, i + 1, (usize) bytes[i]));
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

		usize pos = 0;
		while (pos < bytes.size()) {
			// Check if current byte is not a continuation byte
			if (((bytes[pos] ^ byte{ 0b10000000u }) & byte{ 0b11000000u }) == byte{ 0 }) {
				log.log(base::make_unique<Utf8UnexpectedContinuationError>(
					file, pos + 1, (usize) bytes[pos]
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
				log.log(base::make_unique<Utf8BadByteStartError>(file, pos + 1, (usize) bytes[pos])
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
					log.log(base::make_unique<Utf8BadNonContinuationError>(
						file, new_pos + 1, (usize) bytes[new_pos], pos + 1
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
				log.log(base::make_unique<Utf8BadEofError>(file, bytes.size(), pos + 1));
				pos = bytes.size();
				continue;
			}

			// Check if value is a valid unicode code point
			if (!U_IS_UNICODE_CHAR(value)
			    || (U_GET_GC_MASK(value) & (U_GC_CN_MASK | U_GC_CO_MASK | U_GC_CS_MASK))) {
				log.log(base::make_unique<Utf8UndefinedCodepointError>(file, pos + 1, value));
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
