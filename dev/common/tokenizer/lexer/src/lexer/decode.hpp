#pragma once

#include "char.hpp"

#include <diagnostic/logger.hpp>
#include <filesystem/encoding.hpp>
#include <token_file/forward.hpp>

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
	CharArray decode(Ref<tokenizer::TokenFile> file, dia::Logger& err);

	template<>
	CharArray decode<fs::US_ASCII>(Ref<tokenizer::TokenFile>, dia::Logger&);

	template<>
	CharArray decode<fs::UTF8>(Ref<tokenizer::TokenFile>, dia::Logger&);
}
