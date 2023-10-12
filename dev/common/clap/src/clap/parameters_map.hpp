#pragma once

#include <unordered_set>
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <base/option.hpp>

namespace clap {
	class ParametersMap {
	private:
		base::HashMap<base::StrId, usize> long_names_to_id;
		base::HashMap<char, usize>        short_names_to_id;

		std::unordered_set<usize>         flags;
		base::HashMap<usize, base::StrId> parameters;

		option<usize> to_id(base::StrId name) const;
		option<usize> to_id(const base::RawView& name) const;
		option<usize> to_id(char name) const;
		option<usize> to_id(byte name) const;

		option<base::RawView> get(usize id) const;
		bool                  contains(usize id) const;

	public:
		template<class T>
		option<base::RawView> get(T name) const {
			return to_id(name).flat_map([this](usize id) { return get(id); });
		}

		template<class T>
		bool contains(T name) const {
			return to_id(name).map([this](usize id) { return contains(id); }).value_or(false);
		}

		friend class Config;
	};
}
