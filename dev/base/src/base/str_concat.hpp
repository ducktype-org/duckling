#pragma once

#include <string>
#include <tuple>
#include "type_traits.hpp"
#include "raw_view.hpp"

// #include <concepts>
// #include <type_traits>

namespace base {

	class StrId;

	namespace detail {

		template<typename T>
		requires (!std::is_integral_v<std::remove_reference_t<T>>)
		void strConcat(std::string& out, T&& v) {
			out.append(v);
		}

		inline void strConcat(std::string& out, base::RawView view) {
			out.append(view.stringView());
		}

		// This is forward declaration to prevent circular header dependency thru:
		// string_id.hpp -> maps.hpp -> exceptions.hpp -> str_concat.hpp
		void strConcat(std::string& out, base::StrId str_id);

		inline void strConcat(std::string& out, std::integral auto v) {
			out.append(std::to_string(v));
		}

		inline void strConcat(std::string& out, bool v) {
			out.append(v ? "true" : "false");
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
	 * @FIXME nullptr causes an crash
	 */
	template<typename... T>
	std::string strConcat(T&&... elements) {
		std::string out;
		(detail::strConcat(out, std::forward<T>(elements)), ...);
		return out;
	}

}
