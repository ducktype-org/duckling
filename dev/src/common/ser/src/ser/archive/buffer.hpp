#pragma once

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>
#include <span>

namespace ser::internal {

	template<byte_buffer B>
	[[nodiscard]] constexpr ::std::byte* bufferData(B& b) noexcept {
		return b.data();
	}

	template<byte_buffer B>
	[[nodiscard]] constexpr ::std::size_t bufferSize(const B& b) noexcept {
		return b.size();
	}

	/** @brief `need` is the absolute size the buffer must reach, not an increment. */
	template<resizable_buffer B>
	constexpr Errc bufferEnsure(B& b, ::std::size_t need) {
		if (b.size() < need) b.resize(need);
		return Errc::Ok;
	}

	template<fixed_buffer B>
	constexpr Errc bufferEnsure(const B& b, ::std::size_t need) noexcept {
		return need <= b.size() ? Errc::Ok : Errc::BufferFull;
	}

	/**
	 * @brief where a fresh archive starts writing
	 * @details Same split as bufferEnsure, and for the same reason: a growable buffer's size()
	 * is its CONTENT, so a new ser::out appends after it and the caller can build one
	 * stream out of several independent writes. A fixed buffer's size() is its
	 * CAPACITY - there is nothing to append after - so it is filled from the front.
	 */
	template<resizable_buffer B>
	[[nodiscard]] constexpr ::std::size_t bufferOrigin(const B& b) noexcept {
		return b.size();
	}

	template<fixed_buffer B>
	[[nodiscard]] constexpr ::std::size_t bufferOrigin(const B&) noexcept {
		return 0;
	}

	/** @brief Whether writes to this buffer can fail at all. */
	template<class B>
	inline constexpr bool BUFFER_CAN_FAIL = !resizable_buffer<B>;

} /* namespace ser::internal */
