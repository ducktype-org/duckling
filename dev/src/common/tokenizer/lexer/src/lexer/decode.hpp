#pragma once

#include "char.hpp"

#include <diagnostic_interactive/logger.hpp>

#include <filesystem/encoding.hpp>
#include <token_source/forward.hpp>

namespace lexer {
	/**
	 * Decode an array of bytes using the given encoding
	 *
	 * @tparam encoding Which encoding should the function use.
	 * @param bytes View of the bytes to decode
	 * @param err `dia_int::Logger` to store errors
	 *
	 * @return CharArray of decoded data
	 *
	 * @note We should probably stick to only decoding UTF-8 for now
	 */
	template<fs::Encoding encoding>
	CharArray decode(Ref<tokenizer::TokenSource> file);

	template<>
	CharArray decode<fs::UsAscii>(Ref<tokenizer::TokenSource>);

	template<>
	CharArray decode<fs::UTF8>(Ref<tokenizer::TokenSource>);
}
