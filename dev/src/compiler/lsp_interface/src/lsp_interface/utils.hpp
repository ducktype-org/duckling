#pragma once

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>

#include <map>
#include <string>
#include <vector>

namespace lsp {
	/**
	 * @brief Converts a list of strings to a JSON array format.
	 *
	 * @param list The list of strings to convert.
	 * @return std::string The JSON array representation of the list.
	 */
	std::string jsonList(const std::vector<std::string>& list);

	/**
	 * @brief Converts a dictionary of strings to a JSON object format.
	 *
	 * @param dict The dictionary of strings to convert.
	 * @return std::string The JSON object representation of the dictionary.
	 */
	std::string jsonDict(const std::map<std::string, std::string>& dict);

}
