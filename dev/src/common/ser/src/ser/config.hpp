#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__SANITIZE_ADDRESS__)
	#define SER_HAS_ASAN 1
#elif defined(__has_feature)
	#if __has_feature(address_sanitizer)
		#define SER_HAS_ASAN 1
	#else
		#define SER_HAS_ASAN 0
	#endif
#else
	#define SER_HAS_ASAN 0
#endif

#ifndef SER_DEBUG_POISON
	#ifdef NDEBUG
		#define SER_DEBUG_POISON 0
	#elif SER_HAS_ASAN
		#define SER_DEBUG_POISON 2
	#else
		#define SER_DEBUG_POISON 1
	#endif
#endif

namespace ser {
	/**
	 * @brief The version of the serialized format. It goes into every schema hash, so bumping it
	 * makes streams written by an older version unreadable.
	 */
	inline constexpr int VERSION = 1;

	struct config_global final {
		/** @brief use u64 as size_type to avoid silent overflows from size_t conversions */
		using size_type = ::std::uint64_t;

		/** @brief Bounds nesting, so data-dependent recursion cannot overflow the stack. */
		static constexpr ::std::size_t MAX_DEPTH = 256;

		static constexpr ::std::size_t MAX_ZERO_SIZE_ELEMENTS = ::std::size_t{ 1 } << 28;

		// @TODO: #90001 align flat arrays of trivially copyable elements for zero-copy reads
	};

} /* namespace ser */
