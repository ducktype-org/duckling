#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>

#include <frontend/module_tree/module_tree.hpp>

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

	/**
	 * @brief Recursively puts Duckling files in the virtual file system.
	 *
	 * @param path Path to the file/folder to be added to the vfs and continue recursion from.
	 * @param virtual_root The root of the virtual file system.
	 * @return Path to the newly created file/folder in the virtual file system.
	 */
	fs::FilePath initFiles(const fs::FilePath& path, const fs::File& virtual_root);

	/**
	 * @brief Recursively finds Duckling module files and adds them to the module tree.
	 *
	 * @param path Path to the file/folder to be searched for module files.
	 *
	 * @note Only files with the .dmf extension are considered module files.
	 * @note In the LS daemon context, the path should be within the virtual file system.
	 */
	void initModules(const fs::FilePath& path);

	/**
	 * @brief Recursively queries PSTs of the files.
	 *
	 * @param path Path to the file/folder to query the PST for or continue recursion from.
	 *
	 * @note The PSTs are queried only for Duckling files.
	 * @note In the LS daemon context, the path should be within the virtual file system.
	 */
	void initPSTs(const fs::FilePath& path);

	/**
	 * @brief Puts or updates a file in the virtual file system, notifies module tree and updates
	 * its PST.
	 *
	 * @param virtual_root The root of the virtual file system.
	 * @param path The path to the file to be updated.
	 * @param content The content to write to the vfs.
	 */
	void putFile(const fs::File& virtual_root, const std::string& path, const std::string& content);


	bool isModuleTreeParsedSuccessfully(base::CRef<compiler::frontend::ModuleTree> module);
	
	base::CRef<compiler::frontend::ModuleTree> getRootModule(compiler::frontend::ModuleID module_id);

	bool isPackageParsedSuccessfully(compiler::frontend::ModuleID module_id);


}
