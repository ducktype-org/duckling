/**
 * @file string_id.hpp
 *
 * @brief String ID is a library that implements `StrId` type.
 * It is a light-weight representation of string.
 *
 * @attention String id uses global data, and therefore should not be used
 * "before" `main`. It may lead to static Initialization Order Fiasco.
 *
 * Functionalities
 * ===============
 *
 * StrId creation
 * --------------
 *
 * `StrId` can be created from various other string representations using `StrId` constructors.
 *
 * Bad state of StrId
 * ------------------
 *
 * `StrId` default constructor leaves it in a "bad" state, which does not
 * represent any string. Whether `StrId` is in a bad or good state can be
 * checked using `.isGood()` and `.isBad()` methods.
 *
 * Retrieving original string
 * --------------------------
 *
 * String represented by `StrId` can be accessed directly using `.view()`,
 * `.strView()` and `.str()` methods.
 *
 * @note `.str()` constructs a new string, while other methods provide only a view to existing data.
 *
 * Testing for equality
 * --------------------
 *
 * Using `==` operator on `StrId` is equivalent to testing equality of represented
 * strings (Same for `!=`).
 *
 * @note `StrId == StrId` is extremely quick.
 *
 * Comparison
 * ----------
 *
 * Using `<` operator on `StrId` will provide a well-behaving linear order.
 * It can be used with, for example, `std::map`.
 *
 * @attention Order used by `<` is arbitrary and has nothing to do with lexicographical comparison.
 *
 * Additional functionalities
 * --------------------------
 *
 * * `StrId` can be converted to `usize` representation with `strIdToNum`. It is mostly for debug or
 * strange quick hacks.
 * * `StrId` can be hashed using standard `std::hash`.
 * * `StrId` works with `base::strConcat`.
 */

#pragma once

#include "strongly_typed_id.hpp"
#include "maps.hpp"
#include "raw_view.hpp"
#include <string>
#include <charconv>

namespace base {

	namespace detail {
		STRONG_TYPEDEF_ID(StrInnerID);
	}

	class StrId {
		using InnerId = detail::StrInnerID;
		InnerId id;

		using ToDataType = VectorMap<InnerId, RawView>;
		using ToIdType   = HashMap<RawView, InnerId>;

		static ToDataType to_data_map;
		static ToIdType   to_id_map;

	public:
		StrId(): id(InnerId::bad()){};
		StrId(const StrId& oth) = default;
		StrId(StrId&& oth)      = default;

		explicit StrId(char character);

		// Makes copy
		explicit StrId(const RawView& data);
		explicit StrId(const char* data);

		StrId& operator=(const StrId& oth) = default;

		[[nodiscard]]
		base::RawView view() const {
			RIFT_ASSERT(id.isGood(), "StrId is bad");
			return to_data_map[id];
		}

		[[nodiscard]]
		std::string_view strView() const {
			return view().stringView();
		}

		[[nodiscard]]
		std::string str() const {
			return view().stdString();
		}

		[[nodiscard]]
		bool isBad() const {
			return id.isBad();
		}

		[[nodiscard]]
		bool isGood() const {
			return !id.isBad();
		}

		std::strong_ordering operator<=>(const StrId& oth) const = default;

		/**
		 * Due to the operator==(RawView) definition, implicit operator==(StrId) is deleted.
		 * Here, it is defined explicitly.
		 */
		bool operator==(const StrId& oth) const {
			return operator<=>(oth) == std::strong_ordering::equal;
		}

		bool operator==(const RawView& oth) const {
			return view().stringView() == oth.stringView();
		}

		explicit operator usize() const { return usize(id); }

		friend void swap(StrId& first, StrId& second) {
			using std::swap;
			swap(first.id, second.id);
		}

		/**
		 * For debug purposes
		 * Prints content of StrId inner data
		 */
		static void dumpData(std::ostream& out);

		/**
		 * For debug purposes
		 * Get internal ID
		 */
		[[nodiscard]]
		InnerId innerID() const {
			return this->id;
		}

		friend class std::hash<StrId>;
	};

	/**
	 * @brief Converts a string to the value of the number it contains.
	 *
	 * Raises exception on error.
	 */
	i64 strIdToNum(base::StrId str);

	namespace detail {
		inline void strConcat(std::string& out, StrId str_id) { out.append(str_id.strView()); }
	}
}

namespace std {
	template<>
	struct hash<base::StrId> {
		usize operator()(const base::StrId& x) const { return x.id.asInt(); }
	};
}
