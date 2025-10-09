#include "vmvalue.hpp"

#include <vm/core/process/vmprocess.hpp>

#include <ostream>

namespace {
	void hexdump(std::ostream& out, const byte* ptr, usize buflen, const std::string& indent) {
		const auto* buf = reinterpret_cast<const unsigned char*>(ptr);
		int         i = 0, j = 0;
		for (i = 0; i < buflen; i += 16) {
			out << indent << std::hex << std::setw(6) << std::setfill('0') << i << ": ";
			for (j = 0; j < 16; j++) {
				if (i + j < buflen) {
					out << std::hex << std::setw(2) << std::setfill('0')
						<< static_cast<int>(buf[i + j]) << " ";
				} else {
					out << "   ";
				}
			}
			out << " ";
			for (j = 0; j < 16; j++)
				if (i + j < buflen)
					out << (std::isprint(buf[i + j]) ? static_cast<char>(buf[i + j]) : '.');
			out << '\n';
		}
	}
}

vm::VmValue::VmValue(VMProcess& process, TypeCRef type):
	  data(type->getSize()),
	  my_process(&process),
	  memory(&process.memory),
	  type(type),
	  pointer(memory->allocateDummy(type, data.data()), 0) {
	memory->increaseBlockRefcount(pointer.getBlock());
}

vm::VmValue::VmValue(VMProcess& process, TypeCRef type, Pointer src): VmValue(process, type) {
	importData(src);
}

vm::VmValue::~VmValue() {
	if (!pointer.isNull()) std::cerr << "VmValue not freed!\n";
}

void vm::VmValue::exportData(Pointer dst) const { memory->copyPointedData(dst, pointer, type); }

void vm::VmValue::importData(Pointer src) { memory->copyPointedData(pointer, src, type); }

void vm::VmValue::freeData() {
	memory->freeBlockData(pointer.getBlock());
	memory->decreaseBlockRefcount(pointer.getBlock());
	pointer = Pointer::null();
}

vm::PID vm::VmValue::getPID() const { return my_process->my_pid; }

byte* vm::VmValue::getBytes() { return data.data(); }

const byte* vm::VmValue::getBytes() const { return data.data(); }

void vm::VmValue::dprint(std::ostream& out, const std::string& indent) const {
	out << indent << "---- VmValue ----\n";
	out << indent << "Type: " << type->getName().str() << " (Size: " << type->getSize()
		<< " bytes)\n";
	out << indent << "Value:";

	const auto type_name = type->getName();

	if (type_name == base::StrID("void"))
		out << " <void>\n";
	else if (type_name == base::StrID("i64"))
		out << " " << readBytes<i64>() << " (as i64)\n";
	else if (type_name == base::StrID("i32"))
		out << " " << readBytes<i32>() << " (as i32)\n";
	else if (type_name == base::StrID("i16"))
		out << " " << readBytes<i16>() << " (as i16)\n";
	else if (type_name == base::StrID("byte"))
		out << " " << readBytes<char>() << " (as byte)\n";
	else
		out << " <Unable to interpret>\n";

	out << "Bytes:\n";
	hexdump(out, data.data(), data.size(), indent);
	out << indent << "-----------------\n";
}
