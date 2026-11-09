// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "hashing_algorithms.hpp"

#if DUCK_SHA256_HARDWARE_X86

	#include <cpuid.h>
	#include <immintrin.h>

namespace hashing::internal {

	bool sha256HardwareAvailable() noexcept {
		unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
		if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return false;
		const bool ssse3  = (ecx & bit_SSSE3) != 0;
		const bool sse4_1 = (ecx & bit_SSE4_1) != 0;
		if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) return false;
		const bool sha = (ebx & bit_SHA) != 0;
		return ssse3 && sse4_1 && sha;
	}

	/**
	 * SHA-256 block transform with the x86 SHA extensions. Processes the block in 16 groups of 4
	 * rounds, keeping the message schedule in 4 rotating registers.
	 */
	__attribute__((target("sha,sse4.1,ssse3"))) void sha256TransformHardware(
		u32* state, const std::byte* block
	) noexcept {
		alignas(16) static constexpr u32 K[64] = {
			0x42'8a'2f'98, 0x71'37'44'91, 0xb5'c0'fb'cf, 0xe9'b5'db'a5, 0x39'56'c2'5b,
			0x59'f1'11'f1, 0x92'3f'82'a4, 0xab'1c'5e'd5, 0xd8'07'aa'98, 0x12'83'5b'01,
			0x24'31'85'be, 0x55'0c'7d'c3, 0x72'be'5d'74, 0x80'de'b1'fe, 0x9b'dc'06'a7,
			0xc1'9b'f1'74, 0xe4'9b'69'c1, 0xef'be'47'86, 0x0f'c1'9d'c6, 0x24'0c'a1'cc,
			0x2d'e9'2c'6f, 0x4a'74'84'aa, 0x5c'b0'a9'dc, 0x76'f9'88'da, 0x98'3e'51'52,
			0xa8'31'c6'6d, 0xb0'03'27'c8, 0xbf'59'7f'c7, 0xc6'e0'0b'f3, 0xd5'a7'91'47,
			0x06'ca'63'51, 0x14'29'29'67, 0x27'b7'0a'85, 0x2e'1b'21'38, 0x4d'2c'6d'fc,
			0x53'38'0d'13, 0x65'0a'73'54, 0x76'6a'0a'bb, 0x81'c2'c9'2e, 0x92'72'2c'85,
			0xa2'bf'e8'a1, 0xa8'1a'66'4b, 0xc2'4b'8b'70, 0xc7'6c'51'a3, 0xd1'92'e8'19,
			0xd6'99'06'24, 0xf4'0e'35'85, 0x10'6a'a0'70, 0x19'a4'c1'16, 0x1e'37'6c'08,
			0x27'48'77'4c, 0x34'b0'bc'b5, 0x39'1c'0c'b3, 0x4e'd8'aa'4a, 0x5b'9c'ca'4f,
			0x68'2e'6f'f3, 0x74'8f'82'ee, 0x78'a5'63'6f, 0x84'c8'78'14, 0x8c'c7'02'08,
			0x90'be'ff'fa, 0xa4'50'6c'eb, 0xbe'f9'a3'f7, 0xc6'71'78'f2,
		};

		// Big endian 32-bit words of the message.
		const __m128i byte_swap
			= _mm_set_epi64x(0x0c'0d'0e'0f'08'09'0a'0bULL, 0x04'05'06'07'00'01'02'03ULL);

		// Load the state into the ABEF / CDGH layout used by sha256rnds2.
		__m128i tmp    = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state));
		__m128i state1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state + 4));
		tmp            = _mm_shuffle_epi32(tmp, 0xB1);        // CDAB
		state1         = _mm_shuffle_epi32(state1, 0x1B);     // EFGH
		__m128i state0 = _mm_alignr_epi8(tmp, state1, 8);     // ABEF
		state1         = _mm_blend_epi16(state1, tmp, 0xF0);  // CDGH

		const __m128i abef_save = state0;
		const __m128i cdgh_save = state1;

		__m128i msgs[4];
		for (int i = 0; i < 16; i++) {
			if (i < 4) {
				msgs[i] = _mm_shuffle_epi8(
					_mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 16 * i)), byte_swap
				);
			}
			__m128i& cur = msgs[i % 4];

			__m128i msg
				= _mm_add_epi32(cur, _mm_load_si128(reinterpret_cast<const __m128i*>(&K[4 * i])));
			state1 = _mm_sha256rnds2_epu32(state1, state0, msg);

			// Finish the message schedule of group i + 1.
			if (i >= 3 && i < 15) {
				__m128i& next = msgs[(i + 1) % 4];
				next          = _mm_add_epi32(next, _mm_alignr_epi8(cur, msgs[(i + 3) % 4], 4));
				next          = _mm_sha256msg2_epu32(next, cur);
			}

			msg    = _mm_shuffle_epi32(msg, 0x0E);
			state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

			// Start the message schedule of group i + 3.
			if (i >= 1 && i < 13) {
				__m128i& prev = msgs[(i + 3) % 4];
				prev          = _mm_sha256msg1_epu32(prev, cur);
			}
		}

		state0 = _mm_add_epi32(state0, abef_save);
		state1 = _mm_add_epi32(state1, cdgh_save);

		tmp    = _mm_shuffle_epi32(state0, 0x1B);     // FEBA
		state1 = _mm_shuffle_epi32(state1, 0xB1);     // DCHG
		state0 = _mm_blend_epi16(tmp, state1, 0xF0);  // DCBA
		state1 = _mm_alignr_epi8(state1, tmp, 8);     // HGFE

		_mm_storeu_si128(reinterpret_cast<__m128i*>(state), state0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(state + 4), state1);
	}
}

#endif
