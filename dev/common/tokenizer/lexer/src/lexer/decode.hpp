#pragma once

#include "char.hpp"
#include <diagnostic/error_state.hpp>
#include <filesystem/encoding.hpp>

namespace lexer {
	/**
	 * Decode an array of bytes using given encoding
	 * 
	 * @tparam encoding Which encoding should the function use.
	 * @param bytes View of the bytes to decode
	 * @param err `dia::ErrorState` to store errors
	 * 
	 * @return CharArray of decoded data
	 * 
	 * @note We should probably stick to only decoding UTF-8 for now
	 */
	template<fs::Encoding encoding>
	CharArray decode(base::RawView bytes, dia::ErrorState& err);

	template<>
	CharArray decode<fs::US_ASCII>(base::RawView bytes, dia::ErrorState&);

	template<>
	CharArray decode<fs::UTF8>(base::RawView bytes, dia::ErrorState&);
}