#include <base/except/exceptions.hpp>

#include <bit>
#include <cstdint>
#include <functional>

namespace vm::jit::cnp::internal {
	struct OpaqueStruct;

	template<class Type, class IntegerEquivalent>
	Type valueFromPointer(OpaqueStruct& link_time_variable) {
		auto value = std::bit_cast<uintptr_t>(&link_time_variable);

		CORE_ASSERT(
			sizeof(IntegerEquivalent) <= sizeof(intptr_t), "Type too big for a link-time constant"
		);
		auto truncated_value = static_cast<IntegerEquivalent>(value);

		CORE_ASSERT(sizeof(IntegerEquivalent) == sizeof(Type), "Wrong size of link-time constant");
		return std::bit_cast<Type>(truncated_value);
	}
}

#define LINK_VARIABLE_NAME(name) _##name
#define DECLARE_LINK_VARIABLE(name) \
	extern "C" { __attribute__((weak)) extern internal::OpaqueStruct LINK_VARIABLE_NAME(name); }
#define GET_LINK_VARIABLE(name, type, size) \
	internal::valueFromPointer<type, u##size>(LINK_VARIABLE_NAME(name))

#define LINK_VALUE(name, type, size)                \
	std::invoke([] {                                \
		DECLARE_LINK_VARIABLE(name);                \
		return GET_LINK_VARIABLE(name, type, size); \
	})
