#include "c_decls.hpp"

#include <utility>

namespace c_import {

	CType makeScalar(Scalar scalar) { return CType{ ScalarType{ scalar } }; }

	CType makePointer(CType pointee) {
		return CType{ PointerType{ std::make_shared<const CType>(std::move(pointee)) } };
	}

	CType makeOpaquePointer() { return CType{ PointerType{ nullptr } }; }

	CType makeRecord(std::string emitted_name) {
		return CType{ RecordType{ std::move(emitted_name) } };
	}

	CType makeArray(CType element, std::uint64_t count) {
		return CType{ ArrayType{ .element = std::make_shared<const CType>(std::move(element)),
			                     .count   = count } };
	}

	CType makeUnsupported(std::string reason) {
		return CType{ UnsupportedType{ std::move(reason) } };
	}

}
