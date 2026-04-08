/**
 * @file string_id.hpp
 *
 * @brief String ID is a library that implements `StrID` type.
 * It is a light-weight representation of string.
 *
 * @attention String id uses global data, and therefore should not be used
 * "before" `main`. It may lead to static Initialization Order Fiasco.
 *
 * Functionalities
 * ===============
 *
 * StrID creation
 * --------------
 *
 * `StrID` can be created from various other string representations using `StrID` constructors.
 *
 * Bad state of StrID
 * ------------------
 *
 * `StrID` default constructor leaves it in a "bad" state, which does not
 * represent any string. Whether `StrID` is in a bad or good state can be
 * checked using `.isGood()` and `.isBad()` methods.
 *
 * Retrieving original string
 * --------------------------
 *
 * String represented by `StrID` can be accessed directly using `.view()`,
 * `.strView()` and `.str()` methods.
 *
 * @note `.str()` constructs a new string, while other methods provide only a view to existing data.
 *
 * Testing for equality
 * --------------------
 *
 * Using `==` operator on `StrID` is equivalent to testing equality of represented
 * strings (Same for `!=`).
 *
 * @note `StrID == StrID` is extremely quick.
 *
 * Comparison
 * ----------
 *
 * Using `<` operator on `StrID` will provide a well-behaving linear order.
 * It can be used with, for example, `std::map`.
 *
 * @attention Order used by `<` is arbitrary and has nothing to do with lexicographical comparison.
 *
 * Additional functionalities
 * --------------------------
 *
 * * `StrID` can be converted to `usize` representation with `strIDToNum`. It is mostly for debug or
 * strange quick hacks.
 * * `StrID` can be hashed using standard `std::hash`.
 * * `StrID` works with `base::strConcat`.
 */

#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/misc/raw_view.hpp>

#include <hashing/add_to_hash.hpp>

#include <charconv>
#include <string>
#include <type_traits>

namespace base {

	namespace internal {
		STRONG_TYPEDEF_ID(StrInnerID);
	}

	/**
	 * @brief String-ID utility used widely as keys (artifacts, module names, symbols, etc.).
	 * @note It is thread-safe.
	 */
	class StrID final {
	public:
		using InnerID = internal::StrInnerID;

	private:
		InnerID id;

	public:
		StrID(): id(InnerID::bad()) {}

		StrID(const StrID& oth) = default;
		StrID(StrID&& oth)      = default;

		explicit StrID(char character);

		// Makes copy
		explicit StrID(const RawView& data);
		explicit StrID(const char* data);
		explicit StrID(const std::string& data);

		StrID& operator=(const StrID& oth) = default;

		[[nodiscard]]
		base::RawView view() const;

		[[nodiscard]]
		std::string_view strView() const {
			return view().stringView();
		}

		[[nodiscard]]
		std::string str() const {
			return view().stdString();
		}

		// Ranges-friendly API (contiguous range over characters)
		[[nodiscard]]
		usize size() const noexcept {
			return view().stringView().size();
		}

		[[nodiscard]]
		const char* data() const noexcept {
			return strView().data();
		}

		[[nodiscard]]
		const char* begin() const noexcept {
			return data();
		}

		[[nodiscard]]
		const char* end() const noexcept {
			return data() + size();
		}

		[[nodiscard]]
		bool isBad() const {
			return id.isBad();
		}

		[[nodiscard]]
		bool isGood() const {
			return !id.isBad();
		}

		auto operator<=>(const StrID& oth) const = default;

		/**
		 * Due to the operator==(RawView) definition, implicit operator==(StrID) is deleted.
		 * Here, it is defined explicitly.
		 */
		bool operator==(const StrID& oth) const {
			return operator<=>(oth) == std::strong_ordering::equal;
		}

		bool operator==(const RawView& oth) const {
			return view().stringView() == oth.stringView();
		}

		explicit operator usize() const { return usize(id); }

		friend void swap(StrID& first, StrID& second) noexcept {
			using std::swap;
			swap(first.id, second.id);
		}

		/**
		 * For debug purposes
		 * Prints content of StrID inner data
		 */
		static void dumpData(std::ostream& out);

		/**
		 * For debug purposes
		 * Get internal ID
		 */
		[[nodiscard]]
		InnerID getInnerID() const {
			return this->id;
		}

		void addToHash(hashing::hash_algorithm auto& hash_alg) const {
			hashing::addToHash(hash_alg, strView().size());
			hashing::addToHash(hash_alg, strView());  // does it work??
		}

		friend struct std::hash<StrID>;
	};

	/**
	 * @brief Converts a string to the value of the number it contains.
	 *
	 * Raises exception on error.
	 */
	template<class T = i64>
	requires std::is_arithmetic_v<T> T strIDToNum(base::StrID str) {
		auto view = str.strView();

		T                      out{};
		std::from_chars_result res = std::from_chars(view.data(), view.data() + view.size(), out);
		if (res.ec == std::errc::invalid_argument)
			throw std::invalid_argument{ "invalid_argument" };
		else if (res.ec == std::errc::result_out_of_range)
			throw std::out_of_range{ "out_of_range" };

		return out;
	}

	namespace internal {
		inline void strConcat(std::string& out, StrID str_id) { out.append(str_id.strView()); }
	}
}

namespace std {
	template<>
	struct hash<base::StrID> final {
		usize operator()(const base::StrID& x) const { return x.id.asInt(); }
	};
}
