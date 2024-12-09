#include "proc_io.hpp"
#include <mutex>
#include <base/exceptions.hpp>

vm::ProcIORedirecter vm::ProcIO::attach(std::istream& input_source, std::ostream& output_dst) {
	std::unique_lock lock(iomutex);
	CORE_ASSERT(!attached, "This is not possible");
	return { *this, input_source, output_dst };
}
