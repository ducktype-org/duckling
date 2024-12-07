#include "proc_io.hpp"

vm::ProcIORedirecter vm::ProcIO::attach(std::istream& input_source, std::ostream& output_dst) {
	return { *this, input_source, output_dst };
}
