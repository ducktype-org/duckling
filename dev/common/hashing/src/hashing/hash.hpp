#pragma once

#include "unique_id.hpp"
#include "hash_utils.hpp"
#include "hashing_algorithms.hpp"

namespace hashing {


	template<hash_algorithm HashAlgorithm, typename T>
	constexpr void add_to_hash(HashAlgorithm& h, const T& t)
		requires detail::can_hash<HashAlgorithm, T> || detail::can_hash_decompose<T>
	          || std::is_floating_point_v<T> {
		if constexpr (detail::can_hash<HashAlgorithm, T>) {
			h(t);
		} else if constexpr (detail::can_hash_decompose<T>) {
			std::apply([&](auto&&... args) { (add_to_hash(h, args), ...); }, hash_decompose(t));
		} else if constexpr (std::is_floating_point_v<T>) {
			auto t_ = auto{ t };
			if (t_ == 0.0f) t_ = 0.0f;
			std::array<char, sizeof(t_)> arr = std::bit_cast<std::array<char, sizeof(t_)>, T>(t_);
			h(arr.data(), arr.size());
		} else {
			static_assert(false, "Please provide a specialization for this type");
		}
	}

	namespace detail {
		static constexpr bool AllowForStdHash = true;

		auto hash_decompose(const auto&);

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void apply_hash(HashAlgorithm& h, const T& t) {
			if constexpr (requires { add_to_hash(h, t); }) {
				add_to_hash(h, t);
			} else if constexpr (requires { hash_decompose(t); }) {
				apply_hash(h, t);
			} else if constexpr (detail::AllowForStdHash && requires { std::hash<T>{}(t); }) {
				h(std::hash<T>{}(t));
			} else {
				static_assert(
					false,
					"Please provide an 'add_to_hash' or 'hash_decompose' overload for this type"
				);
			}
		}
	}  // namespace detail

	template<hash_algorithm HashAlgorithm = fnv1a_64, bool AppendTypeHashCode = true>
	class hash {
	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h;
			detail::apply_hash(h, t);

			if constexpr (AppendTypeHashCode) h(type_hash_code<T>);

			return static_cast<result_type>(h);
		}
	};

	template<hash_algorithm HashAlgorithm = fnv1a_64, bool AppendTypeHashCode = true>
	class stateful_hash {
		HashAlgorithm h;

	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) noexcept {
			detail::apply_hash(h, t);

			if constexpr (AppendTypeHashCode) h(type_hash_code<T>);

			return static_cast<result_type>(h);
		}

		template<typename... Ts>
		constexpr result_type operator()(const Ts&... ts) noexcept {
			if constexpr (AppendTypeHashCode)
				((detail::apply_hash(h, ts), detail::apply_hash(h, type_hash_code<Ts>)), ...);
			else
				(detail::apply_hash(h, ts), ...);

			return static_cast<result_type>(h);
		}

		constexpr explicit operator result_type() noexcept { return static_cast<result_type>(h); }
	};


}  // namespace hashing
