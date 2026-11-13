// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "safe_vmvalue.hpp"

#include <logger/logger.hpp>

#include <vm/core/safe/memory/local_slot_block.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>

#include <cstring>
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
	  value_bytes(makeSharedBox<SafeVMValueBytes>(type->getSize().asInt())),
	  my_process(&process),
	  memory(&process.getMemory()),
	  type(type) {}

vm::SafeVMValue::~SafeVMValue() {
	if (!isFreed()) CORE_DEV_LOG(DVM, "VMValue not freed!\n");
}

void vm::SafeVMValue::checkNotFreed() const {
	if (isFreed()) throw exceptions::VMUseAfterFreeException();
}

vm::Pointer vm::SafeVMValue::ensurePointer() {
	if (block == nullptr) {
		// The bytes are already a valid value (zeroed or copied in), so the block adopts them.
		auto new_block = memory->adoptDummy(type, storage());
		Memory::increaseBlockRefcount(new_block);
		block = new_block.get();
	}
	return { Ref(block), 0 };
}

void vm::SafeVMValue::importBytes(const byte* src) {
	if (block != nullptr) {
		memory->copyBytesIntoPointedData({ Ref(block), 0 }, src, type);
		return;
	}
	memory->overwriteBytes(bytesView(), src, type);
}

void vm::SafeVMValue::importFrom(Block* src_block, const byte* src_bytes) {
	if (src_block != nullptr) {
		importData({ Ref(src_block), 0 });
		return;
	}
	importBytes(src_bytes);
}

void vm::SafeVMValue::exportData(Pointer dst) const {
	checkNotFreed();
	if (block != nullptr) {
		memory->copyPointedData(dst, { Ref(block), 0 }, type);
		return;
	}
	memory->copyBytesIntoPointedData(dst, storage(), type);
}

void vm::SafeVMValue::importData(Pointer src) {
	checkNotFreed();
	const usize size     = type->getSize().asInt();
	const auto  src_view = memory->getPointerData(src, size);
	if (Memory::hasChildBlocks(src, size)) {
		memory->copyPointedData(ensurePointer(), src, type);
		return;
	}
	importBytes(src_view.getBegin());
}

void vm::SafeVMValue::exportToSlot(LocalSlot& slot) const {
	checkNotFreed();
	if (slot.block != nullptr) {
		exportData({ Ref(slot.block), 0 });
		return;
	}
	if (block != nullptr && Memory::hasChildBlocks({ Ref(block), 0 }, type->getSize().asInt())) {
		// The child blocks need a block to hang from.
		exportData({ createLocalSlotBlock(slot, *memory), 0 });
		return;
	}
	memory->overwriteBytes({ slot.data, type->getSize().asInt() }, storage(), type);
}

void vm::SafeVMValue::importFromSlot(const LocalSlot& slot) {
	checkNotFreed();
	importFrom(slot.block, slot.data);
}

void vm::SafeVMValue::importDataFrom(const IVMValue& source) {
	const auto* safe_source = dynamic_cast<const SafeVMValue*>(&source);
	CORE_ASSERT(safe_source != nullptr, "Importing data from a value of another VM implementation!");
	CORE_ASSERT(
		safe_source->getPID() == getPID(), "Importing data from a value of another VM process!"
	);
	checkNotFreed();
	safe_source->checkNotFreed();
	if (safe_source == this) return;
	importFrom(safe_source->block, safe_source->storage());
}

vm::SafeVMValueRef vm::SafeVMValue::asRef() const {
	if (block != nullptr) return { *my_process.get(), type, Pointer(Ref(block), 0) };
	return { *my_process.get(), type, ValueBytesLocation{ .bytes = value_bytes, .offset = 0 } };
}

void vm::SafeVMValue::freeData() {
	checkNotFreed();
	if (block != nullptr) {
		memory->freeBlockData(Ref(block));
		memory->decreaseBlockRefcount(Ref(block));
		block = nullptr;
	} else {
		memory->runDataDestructors(bytesView(), type);
	}
	value_bytes->freed = true;
}

vm::PID vm::SafeVMValue::getPID() const { return my_process->getPID(); }

base::CRef<vm::code::valid_type::ValidType> vm::SafeVMValue::getType() const {
	const auto& types = my_process->loader.getHighProgram()->types();
	return types.at(getTypeID());
}

vm::code::valid_type::ValidTypeID vm::SafeVMValue::getTypeID() const {
	// Safe TypeIDs are asserted (in the type builder) to be numerically equal to ValidTypeIDs.
	return code::valid_type::ValidTypeID(type->getID().asInt());
}

Bytes vm::SafeVMValue::getDataSize() const { return type->getSize(); }

byte* vm::SafeVMValue::getBytes() { return storage(); }

const byte* vm::SafeVMValue::getBytes() const { return storage(); }

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
	hexdump(out, storage(), type->getSize().asInt(), indent);
	out << indent << "-----------------\n";
}

base::Optional<vm::InterpretedDataVariant> vm::SafeVMValue::readData() const {
	return asRef().readData();
}
