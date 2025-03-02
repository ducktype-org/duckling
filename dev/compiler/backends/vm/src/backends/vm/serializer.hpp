#pragma once

#include <iostream>
#include "elements.hpp"

namespace compiler::backend_vm {
	void serialize(const CodeFile& file, std::ostream& out);
}
