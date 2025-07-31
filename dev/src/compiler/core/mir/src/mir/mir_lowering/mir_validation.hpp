#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	/**
	 * @brief Validates function, currently checks whether moves are used correctly.
     * @TODO #858 when move flag will be set, write proper tests.
	 */
    bool validateFunction(const Function&);

}
