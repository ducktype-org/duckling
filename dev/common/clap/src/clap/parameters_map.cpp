#include "parameters_map.hpp"

namespace clap {
	base::Optional<usize> ParametersMap::to_id(base::StrId name) const {
		auto it = long_names_to_id.find(name);
		if (it == long_names_to_id.end()) return {};
		return base::Optional(it->second);
	}

	base::Optional<usize> ParametersMap::to_id(const base::RawView& name) const {
		return to_id(base::StrId(name));
	}

	base::Optional<usize> ParametersMap::to_id(char name) const {
		auto it = short_names_to_id.find(name);
		if (it == short_names_to_id.end()) return {};
		return base::Optional(it->second);
	}

	base::Optional<usize> ParametersMap::to_id(byte name) const { return to_id(char(name)); }

	base::Optional<base::RawView> ParametersMap::get(usize id) const {
		auto it = parameters.find(id);
		if (it == parameters.end()) return {};
		return base::Optional(it->second.view());
	}

	bool ParametersMap::contains(usize id) const {
		return flags.count(id) != 0 || parameters.count(id) != 0;
	}
}
