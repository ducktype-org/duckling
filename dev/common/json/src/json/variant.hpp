#pragma once

#include <variant>
#include <nlohmann/json.hpp>
#include "empty_struct.hpp"
#include <type_traits>

//
// namespace JS {
// 	template<class... Args>
// 	class TypeHandler<std::variant<Args...>> {
// 	public:
// 		static inline Error to(std::variant<Args...>& to, ParseContext& contex) {
// 			return Error::NoError;
// 		}
//
// 		static void from(const std::variant<Args...>& from, Token& token, Serializer& serializer) {
// 			impl::beginObject(token, serializer);
//
// 			static const char name[] = "type";
// 			std::string       value  = std::visit(
//                 [](auto& x) {
//                     using T = std::decay_t<decltype(x)>;
//                     return std::string(TypeParseTraits<T>::name.data());
//                 },
//                 from
//             );
// 			token.name       = DataRef(name);
// 			token.name_type  = Type::Ascii;
// 			token.value.data = value.data();
// 			token.value.size = value.size();
// 			token.value_type = Type::String;
// 			serializer.write(token);
//
// 			std::visit(
// 				[&token, &serializer](auto& x) {
// 					using T = std::decay_t<decltype(x)>;
//
// 					if constexpr (std::is_empty_v<T> || IsEmptySerialization<T>::value) {
// 					} else {
// 						auto members
// 							= Internal::JsonStructBaseDummy<T, T>::js_static_meta_data_info();
// 						using MembersType = decltype(members);
// 						Internal::MemberChecker<T, MembersType, 0, MembersType::size - 1>::
// 							serializeMembers(x, members, token, serializer, "");
// 					}
// 				},
// 				from
// 			);
//
// 			impl::endObject(token, serializer);
// 		}
// 	};
// }


template<typename... Args>
struct nlohmann::adl_serializer<std::variant<Args...>> {
	static void to_json(json& j, const std::variant<Args...>& v) {
		std::visit(
			[&]<typename T2>(T2&& value) {
				using T = std::decay_t<T2>;
				// j["index"] = std::string(TypeParseTraits<T>::name.data());
				if constexpr (std::is_empty_v<T> || IsEmptySerialization<T>::value) {
				} else {
					j["data"] = value;
				}
			},
			v
		);
	}
};
