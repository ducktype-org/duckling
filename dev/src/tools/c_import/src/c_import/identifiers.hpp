#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace c_import {

	/** Whether the identifier is a Duckling keyword and therefore unusable as a symbol name. */
	bool isDucklingKeyword(std::string_view name);

	/**
	 * @brief Prefixes a C tag name so the tag and ordinary namespaces cannot collide.
	 *
	 * C keeps `struct stat` and `int stat()` apart; Duckling has one module scope, so tags
	 * become `struct_stat` / `union_stat` / `enum_stat`.
	 */
	std::string taggedRecordName(std::string_view tag);
	std::string taggedUnionName(std::string_view tag);
	std::string taggedEnumName(std::string_view tag);

	/**
	 * @brief Tracks emitted names so a duplicate is skipped rather than silently shadowing.
	 *
	 * @note A name that is a Duckling keyword is rejected rather than renamed: the symbol name
	 *       is the linked C identifier, so renaming it would break the link.
	 */
	class NameRegistry final {
	public:
		struct Verdict final {
			bool accepted;
			/** Empty when accepted. */
			std::string reason;
		};

		Verdict claim(std::string_view name);

		[[nodiscard]] bool isClaimed(std::string_view name) const;

	private:
		std::unordered_map<std::string, bool> m_claimed;
	};

}
