#pragma once

#include <string>
#include <tuple>
#include <stdexcept>
#include <unicode/unistr.h>
#include "type_traits.hpp"
#include "raw_view.hpp"

namespace base {

	class StrId;

	namespace detail {
		template<typename T>
		requires(!base::IsNumber<std::remove_reference_t<T>> && !std::is_same_v<icu::UnicodeString, std::remove_cvref_t<T>>)
		void strConcat(std::string& out, T&& v) {
			out.append(std::forward<T>(v));
		}

		inline void strConcat(std::string& out, base::RawView view) {
			out.append(view.stringView());
		}

		inline void strConcat(std::string& out, base::IsNumber auto v) {
			out.append(std::to_string(v));
		}

		void strConcat(std::string& out, const icu::UnicodeString& unistr);

		// This is forward declaration to prevent circular header dependency through:
		// string_id.hpp -> maps.hpp -> exceptions.hpp -> str_concat.hpp
		void strConcat(std::string& out, base::StrId str_id);

		inline void strConcat(std::string& out, bool v) { out.append(v ? "true" : "false"); }

		inline void strConcat(std::string& out, const char* v) { 
			if (v == nullptr) throw std::domain_error("strConcat called with nullptr");
			out.append(std::string(v)); }

		template<typename U, typename V>
		requires(std::is_trivially_copyable_v<U> && std::is_trivially_copyable_v<V>)
		inline void strConcat(std::string& out, std::pair<U, V> pair) {
			out += "<";
			strConcat(out, pair.first);
			out += ", ";
			strConcat(out, pair.second);
			out += ">";
		}
	}

	/**
	 * @brief creates std::string from elements
	 * is such way:
	 * std::string out;
	 * out.append(element_0)
	 * out.append(element_1)
	 * ...
	 *
	 * Specialization are provides for:
	 * integer types - uses std::to_string
	 * RawView - uses RawView.str()
	 * std::tuple - uses std::make_from_tuple<std::string>
	 *
	 * Throws std::domain_error on nullptr argument
	 */
	template<typename... T>
	std::string strConcat(T&&... elements) {
		std::string out;
		(detail::strConcat(out, std::forward<T>(elements)), ...);
		return out;
	}

}
