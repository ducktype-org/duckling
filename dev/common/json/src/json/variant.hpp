#pragma once

#include <variant>
#include <json_struct/json_struct.h>
#include <type_traits>
#include "empty_struct.hpp"
#include "type_parse.hpp"
#include "helper.hpp"

namespace JS {
	template<class... Args>
	class TypeHandler<std::variant<Args...>> {
	public:
		static inline Error to(std::variant<Args...>& /* to */, ParseContext& /* context */) {
			return Error::NoError;
		}

		static void from(const std::variant<Args...>& from, Token& token, Serializer& serializer) {
			impl::beginObject(token, serializer);

			static const std::array<char, 5> type_name{"type"};
			std::string       value  = std::visit(
                [](auto& x) {
                    using T = std::decay_t<decltype(x)>;
                    return std::string(TypeParseTraits<T>::name.data());
                },
                from
            );
			token.name       = DataRef(type_name.data());
			token.name_type  = Type::Ascii;
			token.value.data = value.data();
			token.value.size = value.size();
			token.value_type = Type::String;
			serializer.write(token);

			std::visit(
				[&token, &serializer](auto& x) {
					using T = std::decay_t<decltype(x)>;

					if constexpr (std::is_empty_v<T> || IsEmptySerialization<T>::value) {
					} else {
						auto members
							= Internal::JsonStructBaseDummy<T, T>::js_static_meta_data_info();
						using MembersType = decltype(members);
						Internal::MemberChecker<T, MembersType, 0, MembersType::size - 1>::
							serializeMembers(x, members, token, serializer, "");
					}
				},
				from
			);

			impl::endObject(token, serializer);
		}
	};
}
