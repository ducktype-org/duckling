#pragma once

#include "char.hpp"

#include <token_source/forward.hpp>

#include <diagnostic/logger.hpp>
#include <filesystem/encoding.hpp>

namespace lexer {
	/**
	 * Decode an array of bytes using the given encoding
	 *
	 * @tparam encoding Which encoding should the function use.
	 * @param bytes View of the bytes to decode
	 * @param err `dia::Logger` to store errors
	 *
	 * @return CharArray of decoded data
	 *
	 * @note We should probably stick to only decoding UTF-8 for now
	 */
	template<fs::Encoding encoding>
	CharArray decode(Ref<tokenizer::TokenSource> file, Ref<dia::Logger> err);

	template<>
	CharArray decode<fs::US_ASCII>(Ref<tokenizer::TokenSource>, Ref<dia::Logger>);

	template<>
	CharArray decode<fs::UTF8>(Ref<tokenizer::TokenSource>, Ref<dia::Logger>);
}
