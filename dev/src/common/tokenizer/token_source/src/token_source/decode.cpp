#include "source.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/message.hpp>

#include <base/misc/convert.hpp>
#include <base/misc/int_conv.hpp>

#include "lexer/char.hpp"
#include <diagnostic/source_position.hpp>
#include <filesystem/encoding.hpp>
#include <token_source/source.hpp>
#include <unicode_classification/classifications.hpp>

namespace tokenizer {

	class AsciiByteError final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "ascii_decode_error" };
		}

	public:
		AsciiByteError(Ref<tokenizer::TokenSource> file, usize byte, usize bad_byte) {
			dia_int::CodeLocationArgument::FileLocation loc{
				.file = file->getFile().getFilePath().string(), .line = 0, .column = 0
			};
			addArgument<dia_int::CodeLocationArgument>("code_location", std::move(loc));
			addArgument<dia_int::TextArgument>("byte", std::to_string(byte));
			addArgument<dia_int::TextArgument>("bad_byte", base::toHexString(bad_byte, 2));
		}
	};

	class Utf8UnexpectedContinuationError final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "utf8_unexpected_continuation" };
		}

	public:
		Utf8UnexpectedContinuationError(
			Ref<tokenizer::TokenSource> file, usize byte, usize bad_byte
		) {
			dia_int::CodeLocationArgument::FileLocation loc{
				.file = file->getFile().getFilePath().string(), .line = 0, .column = 0
			};
			addArgument<dia_int::CodeLocationArgument>("code_location", std::move(loc));
			addArgument<dia_int::TextArgument>("byte", std::to_string(byte));
			addArgument<dia_int::TextArgument>("bad_byte", base::toHexString(bad_byte, 2));
		}
	};

	class Utf8BadByteStartError final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "utf8_bad_byte_start" };
		}

	public:
		Utf8BadByteStartError(Ref<tokenizer::TokenSource> file, usize byte, usize bad_byte) {
			dia_int::CodeLocationArgument::FileLocation loc{
				.file = file->getFile().getFilePath().string(), .line = 0, .column = 0
			};
			addArgument<dia_int::CodeLocationArgument>("code_location", std::move(loc));
			addArgument<dia_int::TextArgument>("byte", std::to_string(byte));
			addArgument<dia_int::TextArgument>("bad_byte", base::toHexString(bad_byte, 2));
		}
	};

	class Utf8BadNonContinuationError final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "utf8_bad_non_continuation" };
		}

	public:
		Utf8BadNonContinuationError(
			Ref<tokenizer::TokenSource> file, usize byte, usize bad_byte, usize code_point_start
		) {
			dia_int::CodeLocationArgument::FileLocation loc{
				.file = file->getFile().getFilePath().string(), .line = 0, .column = 0
			};
			addArgument<dia_int::CodeLocationArgument>("code_location", std::move(loc));
			addArgument<dia_int::TextArgument>("byte", std::to_string(byte));
			addArgument<dia_int::TextArgument>("bad_byte", base::toHexString(bad_byte, 2));
			addArgument<dia_int::TextArgument>("code_point_start", std::to_string(code_point_start));
		}
	};

	class Utf8BadEofError final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "utf8_bad_eof" };
		}

	public:
		Utf8BadEofError(Ref<tokenizer::TokenSource> file, usize byte, usize code_point_start) {
			dia_int::CodeLocationArgument::FileLocation loc{
				.file = file->getFile().getFilePath().string(), .line = 0, .column = 0
			};
			addArgument<dia_int::CodeLocationArgument>("code_location", std::move(loc));
			addArgument<dia_int::TextArgument>("byte", std::to_string(byte));
			addArgument<dia_int::TextArgument>("code_point_start", std::to_string(code_point_start));
		}
	};

	class Utf8UndefinedCodepointError final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "utf8_undefined_codepoint" };
		}

	public:
		Utf8UndefinedCodepointError(Ref<tokenizer::TokenSource> file, usize byte, UChar32 value) {
			dia_int::CodeLocationArgument::FileLocation loc{
				.file = file->getFile().getFilePath().string(), .line = 0, .column = 0
			};
			addArgument<dia_int::CodeLocationArgument>("code_location", std::move(loc));
			addArgument<dia_int::TextArgument>("byte", std::to_string(byte));
			addArgument<dia_int::TextArgument>(
				"value", base::toHexString(base::safeIntConv<usize>(value))
			);
		}
	};

	template<>
	lexer::CharArray TokenSource::internalDecode<fs::UsAscii>() {
		auto             file  = Ref(this);
		auto             log   = file->getIntLogger();
		auto             bytes = file->getContent().view();
		lexer::CharArray out;
		for (usize i = 0; i < bytes.size(); i++) {
			// Check if valid ascii byte
			if ((bytes[i] & byte{ 0b10000000u }) != byte{ 0 }) {
				log->log(makeBox<AsciiByteError>(file, i + 1, (usize) bytes[i]));
				continue;
			}
			out.emplace_back(UChar32(bytes[i]), u8{ 1 }, i);
		}
		// Add eof value
		out.emplace_back(unicode::Classifications::END_OF_FILE_VALUE, u8{ 0 }, bytes.size());
		return out;
	}

	template<>
	lexer::CharArray TokenSource::internalDecode<fs::UTF8>() {
		auto             file  = Ref(this);
		auto             log   = file->getIntLogger();
		auto             bytes = file->getContent().view();
		lexer::CharArray out;

		usize pos = 0;
		while (pos < bytes.size()) {
			// Check if current byte is not a continuation byte
			if (((bytes[pos] ^ byte{ 0b10000000u }) & byte{ 0b11000000u }) == byte{ 0 }) {
				log->log(makeBox<Utf8UnexpectedContinuationError>(file, pos + 1, (usize) bytes[pos])
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
				log->log(makeBox<Utf8BadByteStartError>(file, pos + 1, (usize) bytes[pos]));
				pos++;
				continue;
			}

			// check continuation bytes for validity and figure out their value
			bool are_bytes_ok = true;
			for (usize new_pos = pos + 1; new_pos < pos + size && new_pos < bytes.size();
			     new_pos++) {
				if ((bytes[new_pos] & byte{ 0b11000000u }) != byte{ 0b10000000 }) {
					are_bytes_ok = false;
					log->log(makeBox<Utf8BadNonContinuationError>(
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
				log->log(makeBox<Utf8BadEofError>(file, bytes.size(), pos + 1));
				pos = bytes.size();
				continue;
			}

			// Check if value is a valid unicode code point
			if (!U_IS_UNICODE_CHAR(value)
			    || (U_GET_GC_MASK(value) & (U_GC_CN_MASK | U_GC_CO_MASK | U_GC_CS_MASK))) {
				log->log(makeBox<Utf8UndefinedCodepointError>(file, pos + 1, value));
				pos += size;
				continue;
			}

			out.emplace_back(value, u8(base::safeIntConv<uint8_t>(size)), pos);
			pos += size;
		}
		// Add eof value
		out.emplace_back(unicode::Classifications::END_OF_FILE_VALUE, u8{ 0 }, bytes.size());

		return out;
	}
}
