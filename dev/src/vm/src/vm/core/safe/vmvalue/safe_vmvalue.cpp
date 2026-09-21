#include "safe_vmvalue.hpp"

#include <logger/logger.hpp>

#include <vm/core/safe/safe_vmprocess.hpp>

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

vm::SafeVMValue::SafeVMValue(SafeVMProcess& process, TypeCRef type):
	  data(type->getSize()),
	  my_process(&process),
	  memory(&process.getMemory()),
	  id(UNREGISTERED_ID),
	  type(type),
	  pointer(memory->allocateDummy(type, data.data()), 0) {
	memory->increaseBlockRefcount(pointer.getBlock());
}

vm::SafeVMValue::SafeVMValue(SafeVMProcess& process, TypeCRef type, Pointer src):
	  SafeVMValue(process, type) {
	importData(src);
}

vm::SafeVMValue::~SafeVMValue() {
	if (!pointer.isNull()) CORE_DEV_LOG(DVM, "VMValue not freed!\n");
}

void vm::SafeVMValue::exportData(Pointer dst) const { memory->copyPointedData(dst, pointer, type); }

void vm::SafeVMValue::importData(Pointer src) { memory->copyPointedData(pointer, src, type); }

void vm::SafeVMValue::importDataFrom(const IVMValue& source) {
	const auto* safe_source = dynamic_cast<const SafeVMValue*>(&source);
	CORE_ASSERT(safe_source != nullptr, "Importing data from a value of another VM implementation!");
	CORE_ASSERT(
		safe_source->getPID() == getPID(), "Importing data from a value of another VM process!"
	);
	importData(safe_source->pointer);
}

vm::SafeVMValueRef vm::SafeVMValue::asRef() const { return { *my_process.get(), type, pointer }; }

void vm::SafeVMValue::freeData() {
	memory->freeBlockData(pointer.getBlock());
	memory->decreaseBlockRefcount(pointer.getBlock());
	pointer = Pointer::null();
}

vm::PID vm::SafeVMValue::getPID() const { return my_process->getPID(); }

u64 vm::SafeVMValue::getValueID() const { return id; }

base::CRef<vm::code::valid_type::ValidType> vm::SafeVMValue::getType() const {
	const auto& types = my_process->loader.getHighProgram()->types();
	return types.at(getTypeID());
}

vm::code::valid_type::ValidTypeID vm::SafeVMValue::getTypeID() const {
	// Safe TypeIDs are asserted (in the type builder) to be numerically equal to ValidTypeIDs.
	return code::valid_type::ValidTypeID(type->getID().asInt());
}

Bytes vm::SafeVMValue::getDataSize() const { return type->getSize(); }

byte* vm::SafeVMValue::getBytes() { return data.data(); }

const byte* vm::SafeVMValue::getBytes() const { return data.data(); }

void vm::SafeVMValue::dprint(std::ostream& out, const std::string& indent) const {
	out << indent << "---- VMValue ----\n";
	out << indent << "Type: " << type->getName().str() << " (Size: " << type->getSize().asInt()
		<< " bytes)\n";
	out << indent << "Value:";

	const auto type_name = type->getName();

	if (type_name == base::StrID("i64"))
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

base::Optional<vm::InterpretedDataVariant> vm::SafeVMValue::readData() const {
	return asRef().readData();
}
