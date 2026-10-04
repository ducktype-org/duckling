#include "proc_io.hpp"

#include <base/except/exceptions.hpp>

vm::ProcIORedirecter vm::ProcIO::attach(std::istream& input_source, std::ostream& output_dst) {
	auto lck = lock();
	CORE_ASSERT(!attached, "This is not possible");
	return { *this, input_source, output_dst };
}
