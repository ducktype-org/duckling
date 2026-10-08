#pragma once

#include <base/comptime/type_list.hpp>

#include <tuple>
#include <type_traits>

/**
 * @file
 * @brief The member types of a `base::FieldDecl` list, with cv and references stripped.
 * @details A header of its own because it needs `<tuple>`, which type_list.hpp avoids on purpose.
 */

namespace base {

	/**
	 * @brief The declared types of a `base::TypeList` of `base::FieldDecl`s, stripped of cv and
	 * references, as a `std::tuple` and as a `base::TypeList`.
	 */
	template<class L>
	struct DeclFieldTypes;

	template<class... Ds>
	struct DeclFieldTypes<TypeList<Ds...>> {
		using Tuple = ::std::tuple<::std::remove_cvref_t<typename Ds::type>...>;
		using List  = TypeList<::std::remove_cvref_t<typename Ds::type>...>;
	};

}  // namespace base
