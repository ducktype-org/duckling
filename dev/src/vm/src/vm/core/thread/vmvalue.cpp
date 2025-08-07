#include "vmvalue.hpp"

#include <vm/core/process/vmprocess.hpp>

vm::VmValue::VmValue(VMProcess& process, TypeCRef type):
	  data(type->getSize()),
	  my_process(&process),
	  memory(&process.memory),
	  type(type),
	  pointer(memory->allocateDummy(type, data.data()), 0) {
	std::cout << "New vmvalue\n";
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
	std::cout << "VMvalue free data\n";
	memory->freeBlock(pointer.getBlock());
	pointer = Pointer::null();
}

vm::PID vm::VmValue::getPID() const { return my_process->my_pid; }

byte* vm::VmValue::getBytes() { return data.data(); }

const byte* vm::VmValue::getBytes() const { return data.data(); }
