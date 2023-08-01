#include "parameters_map.hpp"

namespace clap{
	option<std::size_t> ParametersMap::to_id(base::StrId name) const {
		auto it = long_names_to_id.find(name);
		if (it == long_names_to_id.end()) {
			return none<std::size_t>();
		}
		return it->second;
	}
	
	option<std::size_t> ParametersMap::to_id(const base::RawView& name) const {
		return to_id(base::StrId(name));
	}
	
	option<std::size_t> ParametersMap::to_id(char name) const {
		auto it = short_names_to_id.find(name);
		if (it == short_names_to_id.end()) {
			return none<std::size_t>();
		}
		return it->second;
	}

	option<std::size_t> ParametersMap::to_id(byte name) const {
		return to_id(char(name));
	}


	
	option<base::RawView> ParametersMap::get(std::size_t id) const {
		auto it = parameters.find(id);
		if (it == parameters.end()) {
			return none<base::RawView>();
		}
		return it->second.view();
	}
	
	bool ParametersMap::contains(std::size_t id) const {
		return flags.count(id) != 0 || parameters.count(id) != 0;
	}
}