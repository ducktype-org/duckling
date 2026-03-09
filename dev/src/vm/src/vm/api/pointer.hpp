#pragma once

#include <vm/core/process/memory/pointer.hpp>

#include <json/json.hpp>

namespace vm {
	class VMProcess;
}

namespace vm::api {
	struct Pointer {
	private:
		vm::Pointer pointer;

		Pointer(vm::Pointer pointer): pointer(pointer) {}

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Pointer, pointer);

		friend class vm::VMProcess;

	public:
		[[nodiscard]] bool isNull() const { return pointer.isNull(); }
	};
}
