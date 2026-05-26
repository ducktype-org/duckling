namespace vm::jit::cnp {
	namespace internal {
		struct OpaqueStruct;
	}

#define LINK_VARIABLE_NAME(name)    _##name
#define DECLARE_LINK_VARIABLE(name) extern internal::OpaqueStruct LINK_VARIABLE_NAME(name)
#define GET_LINK_VARIABLE(name, type, size)                                                     \
/*	CORE_ASSERT(size == sizeof(type) * 8, "Wrong size of link-time constant"),                  \
		CORE_ASSERT(sizeof(type) <= sizeof(intptr_t), "Type too big for a link-time constant"), \
*/		std::bit_cast<type>(static_cast<u##size>(reinterpret_cast<uintptr_t>(&LINK_VARIABLE_NAME(name))))
}
