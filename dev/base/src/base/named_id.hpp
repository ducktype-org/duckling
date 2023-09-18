/**
 * @file named_id.hpp
 * @author Andrzej
 * @brief Strongly typed Id-s (based on type parameter)
 *
 * usage:
 *
 * 1) independent:
 * struct MyIdName {};
 * typedef NamedId<MyIdName> MyId
 *
 * 2) dependent:
 * typedef NamedId<Number<1>> MyId;
 *
 * Provides id like type. Default Constructor
 */

/**
 * Number reserving - a place to allocate used numbers:
 * example:
 * 1             - SymbolId
 * 1000 -> 1099  - some strange meta programing
 *
 * Reserved space:
 *
 */

#pragma once

#include "ints.hpp"

#include <functional>
#include <type_traits>

namespace base {
	typedef usize id_t;     //

	template<typename Par>  // parameter
	class NamedId {
		static id_t bad_val;
		static id_t next_val;
		explicit NamedId(id_t id): id{ id } {};

	protected:
		id_t id;

	public:
		typedef NamedId<Par> SelfType;
		typedef Par          ParameterType;
		NamedId(): id(bad_val){};
		NamedId(const SelfType& id)                             = default;
		NamedId(SelfType&& id)                                  = default;

		inline SelfType& operator=(const SelfType&)             = default;
		inline auto      operator<=>(const SelfType& oth) const = default;

		inline bool      operator==(const SelfType& oth) const { return id == oth.id; }

		inline id_t      asInt() const { return id; }

		inline bool      isBad() const { return id == bad_val; }

		inline bool      isGood() const { return id != bad_val; }

		explicit operator id_t() const { return asInt(); }

		inline static SelfType next() { return SelfType(++next_val); }

		inline static SelfType bad() { return SelfType(bad_val); }

		inline static id_t     range() { return next_val; }

		friend void            swap(SelfType& first, SelfType& second) {
            using std::swap;
            swap(first.id, second.id);
		}
	};

	template<typename par>
	id_t NamedId<par>::next_val = 0;
	template<typename par>
	id_t NamedId<par>::bad_val = (id_t) (-1);  // ~ max int

	template<i32 n>
	struct Number {};

	/**
	 * @TODO: Not needed for now
	 */
	/* namespace aux {
	    template<typename T>
	    struct is_named_id : std::false_type { };

	    template<typename p>
	    struct is_named_id<NamedId<p>> : std::true_type { };
	}

	template<typename T>
	concept NamedIdConcept = aux::is_named_id<T>::value;*/

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
