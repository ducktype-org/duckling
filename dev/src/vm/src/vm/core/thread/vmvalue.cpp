#include "vmvalue.hpp"

#include <vm/core/process/vmprocess.hpp>

#include <print>

namespace {
	// TODOP: Remove that.
	void hexdump(const byte* ptr, usize buflen) {
		auto* buf = reinterpret_cast<const unsigned char*>(ptr);
		int   i = 0, j = 0;
		for (i = 0; i < buflen; i += 16) {
			std::print("{:06x}: ", i);
			for (j = 0; j < 16; j++)
				if (i + j < buflen)
					std::print("{:02x} ", buf[i + j]);
				else
					std::print("   ");
			std::print(" ");
			for (j = 0; j < 16; j++)
				if (i + j < buflen) std::print("{:c}", isprint(buf[i + j]) ? buf[i + j] : '.');
			std::println("");
		}
	}
}

vm::VmValue::VmValue(VMProcess& process, TypeCRef type):
	  data(type->getSize()),
	  my_process(&process),
	  memory(&process.memory),
	  type(type),
	  pointer(memory->allocateDummy(type, data.data()), 0) {}

vm::VmValue::VmValue(VMProcess& process, TypeCRef type, Pointer src): VmValue(process, type) {
	importData(src);
}

vm::VmValue::~VmValue() {
	if (!pointer.isNull()) std::cerr << "VmValue not freed!\n";
}

void vm::VmValue::exportData(Pointer dst) const { memory->copyPointedData(dst, pointer, type); }

void vm::VmValue::importData(Pointer src) { memory->copyPointedData(pointer, src, type); }

void vm::VmValue::freeData() {
	memory->freeBlock(pointer.getBlock());
	pointer = Pointer::null();
}

vm::PID vm::VmValue::getPID() const { return my_process->my_pid; }

byte* vm::VmValue::getBytes() { return data.data(); }

const byte* vm::VmValue::getBytes() const { return data.data(); }

// TODOP: Remove that.
void vm::VmValue::dprint() const {
	std::cout << "VmValue of type: " << type->getName().str() << '\n';
	std::cout << "Bytes: \n";
	hexdump(data.data(), data.size());
	std::cout << '\n';
}
