#pragma once

#include <unordered_set>
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <base/option.hpp>

namespace clap{
	class ParametersMap {
		private:
			base::HashMap<base::StrId, std::size_t> long_names_to_id;
			base::HashMap<char, std::size_t> short_names_to_id;
		
			std::unordered_set<std::size_t> flags;
			base::HashMap<std::size_t, base::StrId> parameters;
			
			option<std::size_t> to_id(base::StrId name) const;
			option<std::size_t> to_id(const base::RawView& name) const;
			option<std::size_t> to_id(char name) const;
			
			option<base::RawView> get(std::size_t id) const;
			bool contains(std::size_t id) const;
		public:
			template <class T>
			option<base::RawView> get(T name) const {
				return to_id(name).flat_map([this](std::size_t id) {
					return get(id);
				});
			}
			
			template <class T>
			bool contains(T name) const {
				return to_id(name)
					.map([this](std::size_t id) {
						return contains(id);
					})
					.value_or(false);
			}
			
			friend class Config;
	};
}
