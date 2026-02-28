#include "../hash_algorithm_utils.hpp"

#include <base/types/bit256.hpp>

#include <iostream>

namespace hashing {
	namespace internal {

		/**
		 * Primary template of FNV-1a constants for different integer sizes.
		 * Note that there is no definition - only the specializations are to be used
		 */
		template<typename I>
		class Fnv1a_Constants;

		/**
		 * Specialization of FNV-1a constants for integer sizes <= 32 bits
		 */
		template<std::integral I>
		requires(sizeof(I) <= sizeof(u32)) class Fnv1a_Constants<I> {
		protected:
			static constexpr u32 OFFSET_BASIS = 2'166'136'261u;
			static constexpr u32 FNV_PRIME    = (1u << 24) + (1u << 8) + 0x93u;
		};

		/**
		 * Specialization of FNV-1a constants for integer sizes > 32 bits
		 */
		template<std::integral I>
		requires(sizeof(I) > sizeof(u32)) class Fnv1a_Constants<I> {
		protected:
			static constexpr u64 OFFSET_BASIS = 14'695'981'039'346'656'037ull;
			static constexpr u64 FNV_PRIME    = (1ull << 40) + (1ull << 8) + 0xb3ull;
		};

		using namespace base::literals;

		template<>
		class Fnv1a_Constants<base::Bit256> {
		protected:
			static constexpr base::Bit256 OFFSET_BASIS
				= "0xdd268dbcaac550362d98c384c4e576ccc8b1536847b6bbb31023b4c8caee0535"_Bit256;
			static constexpr base::Bit256 FNV_PRIME
				= "0x0000000000000000000001000000000000000000000000000000000000000163"_Bit256;
		};

	}  // namespace internal

	/**
	 * FNV-1a hash algorithm, fast and simple with reasonably good distribution,
	 * though not meant for cryptographic purposes
	 */
	template<typename I>
	class Fnv1a final: protected internal::Fnv1a_Constants<I> {
		using internal::Fnv1a_Constants<I>::OFFSET_BASIS;
		using internal::Fnv1a_Constants<I>::FNV_PRIME;

		I state = OFFSET_BASIS;

	public:
		constexpr Fnv1a<I>& operator()(internal::span_of_bytes auto span) noexcept {
			for (auto&& c: span) {
				state ^= static_cast<unsigned char>(c);
				state *= FNV_PRIME;
			}
			return *this;
		}

		using result_type = I;

		constexpr Fnv1a() = default;

		constexpr Fnv1a(I state): state(state) {}

		constexpr result_type finalize() const noexcept { return static_cast<result_type>(state); }
	};

	using Fnv1a_32  = Fnv1a<u32>;
	using Fnv1a_64  = Fnv1a<u64>;
	using Fnv1a_256 = Fnv1a<base::Bit256>;
}
