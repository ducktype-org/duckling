#pragma once

#include "hash_algorithm_utils.hpp"

#include <base/comptime/type_traits.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace hashing {

	/**
	 * Hash algorithm that keeps the bytes of the hashed objects
	 * and can be converted to a string that represents the bytes in hex
	 */
	class DebugHash final {
		std::vector<std::tuple<std::vector<char>, usize>> bytes;

	public:
		constexpr void operator()(internal::span_of_bytes auto span) noexcept {
			std::vector<char> vec;
			vec.reserve(span.size());

			for (auto&& c: span) vec.push_back(static_cast<char>(c));
			bytes.emplace_back(std::move(vec), vec.size());
		}

		using result_type = std::string;

		constexpr result_type finalize() {
			std::string ret;
			usize       line = 0, pos = 0;

			constexpr std::string_view RED   = "\033[1;31m";
			constexpr std::string_view RESET = "\033[0m";

			// Note: stringstream is not usable in constexpr
			static constexpr auto APPEND_LINE_NUMBER = [](std::string& str, usize num) {
				str += "line ";
				std::string num_str;
				do {
					num_str += static_cast<char>(static_cast<usize>('0') + num % 10);
					num /= 10;
				} while (num != 0);
				str += std::string(4 - num_str.size(), ' ');
				str += num_str;
				str += ":    ";
			};
			static constexpr auto APPEND_BYTE_HEX = [](std::string& str, std::byte b) {
				constexpr static std::string_view HEX = "0123456789ABCDEF";
				str += HEX[std::to_integer<unsigned>(b >> 4)];
				str += HEX[std::to_integer<unsigned>(b & std::byte{ 0xF })];
			};

			for (auto&& [b, len]: bytes) {
				for (usize i = pos, j = 0; j < len; ++i, ++j) {
					if (i % 16 == 0) {
						if (line != 0) ret += '\n';
						APPEND_LINE_NUMBER(ret, line++);
					}
					if (i == pos) ret += RED;

					APPEND_BYTE_HEX(ret, static_cast<std::byte>(b[j]));
					ret += ' ';
					if (i == pos) ret += RESET;
				}
				pos = (pos + len) % 16;
			}

			return ret;
		}
	};

	/**
	 * SHA-256 hash algorithm implementation.
	 * the names of the variables are copied from official standard
	 * that can be found here: https://nvlpubs.nist.gov/nistpubs/fips/nist.fips.180-4.pdf
	 */
	class SHA256_XD {
	private:
		// Internal state: 8 32-bit words
		std::array<u32, 8> state;

		// Buffer for unprocessed data (512 bits = 64 bytes)
		std::array<std::byte, 64> buffer{};

		// Total message length in bits
		u64 total_bits{ 0 };

		// Current size of the buffer
		size_t buffer_size{ 0 };

	public:
		using result_type = base::Bit256;

		// Constructor - initializes the hash state
		constexpr SHA256() noexcept:
			  state{ // Initial hash values: first 32 bits of the fractional parts of the
			         // square roots of the first 8 primes (2 through 19)
			         0x6a'09'e6'67, 0xbb'67'ae'85, 0x3c'6e'f3'72, 0xa5'4f'f5'3a,
			         0x51'0e'52'7f, 0x9b'05'68'8c, 0x1f'83'd9'ab, 0x5b'e0'cd'19
		      } {}

		// Update state with input data
		constexpr void operator()(internal::span_of_bytes auto data) noexcept {
			total_bits += data.size() * 8;  // Update total bits processed

			const std::byte* ptr       = data.data();
			size_t           remaining = data.size();

			// If we have data in the buffer already, fill it and process if full
			if (buffer_size > 0) {
				size_t to_copy = std::min(static_cast<size_t>(64 - buffer_size), remaining);
				for (size_t i = 0; i < to_copy; ++i) buffer.at(buffer_size + i) = ptr[i];
				buffer_size += to_copy;
				ptr += to_copy;
				remaining -= to_copy;

				if (buffer_size == 64) {
					transform(buffer.data());
					buffer_size = 0;
				}
			}

			// Process full 64-byte blocks directly from the input
			while (remaining >= 64) {
				transform(ptr);
				ptr += 64;
				remaining -= 64;
			}

			// Store any remaining bytes in the buffer for next update
			for (size_t i = 0; i < remaining; ++i) buffer.at(buffer_size + i) = ptr[i];
			buffer_size += remaining;
		}

		// Finalize and return the hash value
		[[nodiscard]]
		constexpr result_type finalize() const noexcept {
			SHA256 copy = *this;
			copy.padAndProcess();
			std::array<u32, 8> ret{};
			std::ranges::copy(copy.state.rbegin(), copy.state.rend(), ret.begin());
			return ret;
		}

	private:
		// Helper methods
		/**
		    This function perform the SHA-256 transformation on a 512-bit block of data.
		    It processes the data and updates the internal state of the hash.
		    The transformation is based on the SHA-256 algorithm and uses bitwise operations
		    and modular arithmetic to compute the hash value.
		    @tparam T The type of the data to be transformed (should be a byte array).
		    @param data Pointer to the 512-bit block of data to be transformed.
		    @note The function uses a series of bitwise operations and modular arithmetic to
		    compute the hash value. It also uses a set of constants defined in the SHA-256
		    algorithm specification.
		 */
		constexpr void transform(const std::byte* data) noexcept {
			// SHA-256 Constants: first 32 bits of the fractional parts of the cube roots
			// of the first 64 prime numbers (2 through 311)
			constexpr std::array<u32, 64> k
				= { 0x42'8a'2f'98, 0x71'37'44'91, 0xb5'c0'fb'cf, 0xe9'b5'db'a5, 0x39'56'c2'5b,
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
				    0x90'be'ff'fa, 0xa4'50'6c'eb, 0xbe'f9'a3'f7, 0xc6'71'78'f2 };

			std::array<u32, 64> w{};

			for (size_t i = 0; i < 16; ++i) {
				w.at(i) = (static_cast<u32>(static_cast<unsigned char>(data[i * 4])) << 24)
				        | (static_cast<u32>(static_cast<unsigned char>(data[i * 4 + 1])) << 16)
				        | (static_cast<u32>(static_cast<unsigned char>(data[i * 4 + 2])) << 8)
				        | (static_cast<u32>(static_cast<unsigned char>(data[i * 4 + 3])));
			}

			for (size_t i = 16; i < 64; ++i) {
				const u32 s0 = rightRotate(w.at(i - 15), 7) ^ rightRotate(w.at(i - 15), 18)
				             ^ (w.at(i - 15) >> 3);
				const u32 s1 = rightRotate(w.at(i - 2), 17) ^ rightRotate(w.at(i - 2), 19)
				             ^ (w.at(i - 2) >> 10);
				w.at(i) = w.at(i - 16) + s0 + w.at(i - 7) + s1;
			}

			u32 a = state.at(0);
			u32 b = state.at(1);
			u32 c = state.at(2);
			u32 d = state.at(3);
			u32 e = state.at(4);
			u32 f = state.at(5);
			u32 g = state.at(6);
			u32 h = state.at(7);

			for (size_t i = 0; i < 64; ++i) {
				const u32 s1    = rightRotate(e, 6) ^ rightRotate(e, 11) ^ rightRotate(e, 25);
				const u32 ch    = (e & f) ^ ((~e) & g);
				const u32 temp1 = h + s1 + ch + k.at(i) + w.at(i);
				const u32 s0    = rightRotate(a, 2) ^ rightRotate(a, 13) ^ rightRotate(a, 22);
				const u32 maj   = (a & b) ^ (a & c) ^ (b & c);
				const u32 temp2 = s0 + maj;

				h = g;
				g = f;
				f = e;
				e = d + temp1;
				d = c;
				c = b;
				b = a;
				a = temp1 + temp2;
			}

			state.at(0) += a;
			state.at(1) += b;
			state.at(2) += c;
			state.at(3) += d;
			state.at(4) += e;
			state.at(5) += f;
			state.at(6) += g;
			state.at(7) += h;
		}

		/**
		 * Pads the buffer and processes the final block.
		 * This function is called at the end of the hashing process
		 * the last 64 bits of sha256 hash are the length of the message
		 * in bits, so we need to add the length to the end of the buffer
		 * and add padding before the length to ensure all the block has excactly 512 bits
		 * (64 bytes)
		 */
		constexpr void padAndProcess() noexcept {
			buffer.at(buffer_size++) = std::byte{ 0x80 };  // Use .at() for bounds checking

			if (buffer_size > 56) {
				while (buffer_size < 64) buffer.at(buffer_size++) = std::byte{ 0 };  // Use .at()
				transform(buffer.data());
				buffer_size = 0;
			}

			while (buffer_size < 56) buffer.at(buffer_size++) = std::byte{ 0 };  // Use .at()

			const u64 bits = total_bits;
			buffer.at(56)  = static_cast<std::byte>((bits >> 56) & 0xFF);
			buffer.at(57)  = static_cast<std::byte>((bits >> 48) & 0xFF);
			buffer.at(58)  = static_cast<std::byte>((bits >> 40) & 0xFF);
			buffer.at(59)  = static_cast<std::byte>((bits >> 32) & 0xFF);
			buffer.at(60)  = static_cast<std::byte>((bits >> 24) & 0xFF);
			buffer.at(61)  = static_cast<std::byte>((bits >> 16) & 0xFF);
			buffer.at(62)  = static_cast<std::byte>((bits >> 8) & 0xFF);
			buffer.at(63)  = static_cast<std::byte>(bits & 0xFF);

			transform(buffer.data());
		}

		static constexpr u32 rightRotate(u32 value, u64 count) noexcept {
			return (value >> count) | (value << (32 - count));
		}
	};


	// SipHash13:
	class SipHash13 final {
private:
    using u8 = std::uint8_t;
    using u64 = std::uint64_t;
 
    static constexpr std::size_t kElemSize = sizeof(u64);
 
    struct State final {
        u64 v0;
        u64 v1;
        u64 v2;
        u64 v3;
    };
 
    [[nodiscard]]
    static constexpr u64 rotl(u64 x, unsigned n) noexcept {
        return (x << n) | (x >> (64u - n));
    }
 
    // The SipRound permutation; `compress!` in sip128.rs.
    static constexpr void compress(State& s) noexcept {
        s.v0 += s.v1;
        s.v1 = rotl(s.v1, 13);
        s.v1 ^= s.v0;
        s.v0 = rotl(s.v0, 32);
        s.v2 += s.v3;
        s.v3 = rotl(s.v3, 16);
        s.v3 ^= s.v2;
        s.v0 += s.v3;
        s.v3 = rotl(s.v3, 21);
        s.v3 ^= s.v0;
        s.v2 += s.v1;
        s.v1 = rotl(s.v1, 17);
        s.v1 ^= s.v2;
        s.v2 = rotl(s.v2, 32);
    }
 
    // `Sip13Rounds`: one compression round, three finalization rounds.
    static constexpr void c_rounds(State& s) noexcept {
        compress(s);
    }
 
    static constexpr void d_rounds(State& s) noexcept {
        compress(s);
        compress(s);
        compress(s);
    }
 
    // Absorb one little-endian 64-bit word.
    static constexpr void absorb(State& s, u64 word) noexcept {
        s.v3 ^= word;
        c_rounds(s);
        s.v0 ^= word;
    }
 
    template <typename Byte>
    [[nodiscard]]
    static constexpr u64 load_le(const Byte* p) noexcept {
        u64 word = 0;
        for (std::size_t i = 0; i < kElemSize; ++i) {
            word |= static_cast<u64>(static_cast<u8>(p[i])) << (8u * i);
        }
        return word;
    }
 
    // The buffered tail, zero-extended to a full word. `tail_len_` is always
    // < kElemSize between calls, so the high bytes read as zero padding.
    [[nodiscard]]
    constexpr u64 tail_word() const noexcept {
        u64 word = 0;
        for (std::size_t i = 0; i < tail_len_; ++i) {
            word |= static_cast<u64>(tail_[i]) << (8u * i);
        }
        return word;
    }
 
    State state_;
    std::array<u8, kElemSize> tail_{};
    std::size_t tail_len_{0};
    std::size_t length_{0};
 
public:
    using result_type = base::Bit128;
 
    struct Parts final {
        u64 lo;  // `_0` in `finish128`
        u64 hi;  // `_1` in `finish128`
    };
 
    // Constructor - initializes the hash state.
    // rustc's `StableHasher::new()` uses keys (0, 0).
    constexpr SipHash13() noexcept
        : SipHash13(0, 0) {}
 
    // `SipHasher128::new_with_keys`. The extra `^ 0xee` on v1 is what selects
    // the 128-bit variant.
    constexpr SipHash13(u64 key0, u64 key1) noexcept
        : state_{key0 ^ 0x736f6d6570736575ULL,
                 key1 ^ 0x646f72616e646f6dULL ^ 0xeeULL,
                 key0 ^ 0x6c7967656e657261ULL,
                 key1 ^ 0x7465646279746573ULL} {}
 
    // Update state with input data
    constexpr void operator()(internal::span_of_bytes auto data) noexcept {
        const auto* const bytes = std::data(data);
        const std::size_t size = std::size(data);
        length_ += size;
 
        std::size_t i = 0;
 
        // Top up a partially filled tail first.
        if (tail_len_ != 0) {
            while (i < size && tail_len_ < kElemSize) {
                tail_[tail_len_++] = static_cast<u8>(bytes[i++]);
            }
            if (tail_len_ < kElemSize) {
                return;
            }
            absorb(state_, load_le(tail_.data()));
            tail_len_ = 0;
        }
 
        // Absorb whole elements directly out of the input.
        for (; size - i >= kElemSize; i += kElemSize) {
            absorb(state_, load_le(bytes + i));
        }
 
        // Buffer the remainder; leaves tail_len_ < kElemSize.
        while (i < size) {
            tail_[tail_len_++] = static_cast<u8>(bytes[i++]);
        }
    }
 
    // `finish128`, minus the consuming-self part: the tail and the length byte
    // are folded into a copy of the state so the hasher stays usable.
    [[nodiscard]]
    constexpr Parts finalize_parts() const noexcept {
        State s = state_;
 
        const u64 b = (static_cast<u64>(length_ & 0xff) << 56) | tail_word();
        s.v3 ^= b;
        c_rounds(s);
        s.v0 ^= b;
 
        s.v2 ^= 0xee;
        d_rounds(s);
        const u64 lo = s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
 
        s.v1 ^= 0xdd;
        d_rounds(s);
        const u64 hi = s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
 
        return Parts{lo, hi};
    }
 
    [[nodiscard]]
    constexpr result_type finalize() const noexcept {
        const Parts parts = finalize_parts();
 
        // Canonical SipHash-128 output order: `_0` little-endian, then `_1`.
        std::array<u8, 16> bytes{};
        for (std::size_t i = 0; i < 8; ++i) {
            bytes[i] = static_cast<u8>(parts.lo >> (8u * i));
            bytes[i + 8] = static_cast<u8>(parts.hi >> (8u * i));
        }
 
        return result_type{bytes};
    }
};

	class SHA256 final {
		// not sha, just siphash wrapper, to compile and test quickly

		SipHash13 siphash_;
	public:
		using result_type = base::Bit256;

		constexpr void operator()(internal::span_of_bytes auto data) noexcept {
			siphash_(data);
		}

		[[nodiscard]]
		constexpr result_type finalize() const noexcept {
			auto final = siphash_.finalize();
			return {final.data.at(0), final.data.at(1)};
		}

	};


	/**
	 * The default hash algorithm
	 */
	using DefaultHashAlgorithm = SHA256;

}  // namespace hashing
