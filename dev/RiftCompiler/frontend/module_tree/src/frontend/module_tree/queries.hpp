#pragma once

#include <query_framework/query_int.hpp>
#include <filesystem/file.hpp>
#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @TODO @FIXME !!! here fs::FilePath hash is not collision free which invalidates 
	 * conditions required by query framework.
	 * It might be hard to maintain this condition and ensure it in a long run.
	 * 
	 * I see two options:
	 *  * create hash system independent of std::hash that we will have to implement
	 *  * relax the condition in query framework (seams better but is not that simple to implement in staticly typed lang)
	 *    One way to do it is to add `static base::HashMap` in QueryImplementation struct that maps keys to ints.
	 *    The problem is that is is extremely inefficient for keys like int.
	 *    Simillar way would be probably to require `hashKey` function in QueryImplementation struct
	 *    that could be easily defaulted to std::hash (or identity) for all obvious types.
	 * 
	 */
	DECLARE_QUERY(GetModuleTreeQuery, fs::FilePath, ModuleId)

}
