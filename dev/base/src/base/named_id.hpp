/**
 * @file named_id.hpp
 * @author Andrzej
 * @brief Strongly typed Id-s (based on type parameter)
 */

#pragma once

#include "ints.hpp"
#include <type_traits>
#include <functional>

namespace base {
	using id_t = usize;

	/**
	 * @brief Id-like type that has valid values from 0 to number of items.
	 * 
	 * Different id types are created by using a different template parameter.
	 * 
	 * Custom id's can also be created using base::Number<[value]>.
	 * 
	 * Id's can be cast to `usize` and than used as keys for a VectorMap efficiently.
	 */
	template<typename Par>  // parameter
	class NamedId {
		static id_t bad_val;
		static id_t next_val;
		explicit NamedId(id_t id): id{ id } {};

	protected:
		id_t id;

	public:
		using SelfType = NamedId<Par>;
		using ParameterType = Par;
		NamedId(): id(bad_val){};
		NamedId(const SelfType& id) = default;
		NamedId(SelfType&& id) noexcept = default;

		inline SelfType& operator=(const SelfType&)             = default;
		inline auto      operator<=>(const SelfType& oth) const = default;

		inline bool operator==(const SelfType& oth) const { return id == oth.id; }

		[[nodiscard]] 
		inline id_t asInt() const { return id; }

		[[nodiscard]] 
		inline bool isBad() const { return id == bad_val; }

		[[nodiscard]] 
		inline bool isGood() const { return id != bad_val; }

		explicit operator id_t() const { return asInt(); }

		inline static SelfType next() { return SelfType(++next_val); }

		inline static SelfType bad() { return SelfType(bad_val); }

		inline static id_t range() { return next_val; }

		friend void swap(SelfType& first, SelfType& second) {
			using std::swap;
			swap(first.id, second.id);
		}
	};

	template<typename par>
	id_t NamedId<par>::next_val = 0;
	template<typename par>
	id_t NamedId<par>::bad_val = (id_t) (-1);  // ~ max int

	/**
	 * @brief Type for creating numbered Id's.
	 */
	template<i32 n>
	struct Number {};
}

namespace std {
	template<typename T>
	struct hash<base::NamedId<T>> {
		usize operator()(const base::NamedId<T>& key) const {
			using std::hash;
			return std::hash<id_t>()(base::id_t(key));
		}
	};
}
