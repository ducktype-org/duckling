#pragma once

#include <base/collections/optional.hpp>

#include <nlohmann/json.hpp>

namespace nlohmann {
	template<typename T>
	struct adl_serializer<base::Optional<T>> {
		static void to_json(  // NOLINT(readability-identifier-naming)
			json&                    j,
			const base::Optional<T>& opt
		) {
			if (opt.has_value())
				j = *opt;
			else
				j = nullptr;
		}

		static void from_json(  // NOLINT(readability-identifier-naming)
			const json&        j,
			base::Optional<T>& opt
		) {
			if (j.is_null())
				opt = std::nullopt;
			else
				opt = j.get<T>();
		}
	};
}
