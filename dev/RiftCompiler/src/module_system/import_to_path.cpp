#include "import_to_path.hpp"

#include <string>

namespace modulesys {
	std::string importToPath(std::string_view local_path, const pst::Import& import) {
		std::string result(local_path);
		if (result[result.length() - 1] == '/') result.pop_back();
		auto& names = import.getNames();
		for (auto& name: names.names) result += "/" + name.value.str();
		if (import.getStar()) result += "/*";
		result += ".rift";
		return result;
	}
}  // namespace modulesys
