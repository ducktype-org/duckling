#include "proc_io.hpp"
#include <mutex>

vm::ProcIORedirecter vm::ProcIO::attach(std::istream& input_source, std::ostream& output_dst) {
	std::unique_lock lock(iomutex);
	return { *this, input_source, output_dst };
}
