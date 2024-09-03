#include "block.hpp"

namespace vm {

	namespace detail {

		// @TODO: too similar to code in type.cpp with witch its now incompatible
		// See: https://github.com/ducktype-org/rift-poc-zpp1/issues/91
		bool typeAtOffset(TypeCRef type, u64 offset, TypeCRef searched) {
			if (offset == 0) {
				if (searched->getSize() > type->getSize())
					return false;
				else if (searched->getId() == type->getId())
					return true;
			}
			switch (type->getKind()) {
			case Type::Kind::DynamicTable:
				// @FIXME: DynamicTable is wrong
			case Type::Kind::Pointer:
			case Type::Kind::Function:
				return false;
			case Type::Kind::StaticTable:
				return typeAtOffset(
					type->getInnerType().value(),
					offset % type->getInnerType().value()->getSize(),
					searched
				);
			case Type::Kind::Variant:
				if (offset <= 8) return false;
				return typeAtOffset(type->getInnerType().value(), offset - 8, searched);
			case Type::Kind::Data:
				// @FIXME: no Data implementation
				throw base::NotYetImplemented("typeAtOffset Data");
			default:
				CORE_PANIC("not implemented");
			}
		}
	}

	Pointer Block::BasePointer() const { return { block_id, start }; }

	base::RawView Block::rawPointer() { return { data, element_type->getSize() }; }

	cpp::result<base::ModRawView, Block::error> Block::derefCheck(TypeCRef u, u64 offset) {
		if (offset < start || offset > end) return cpp::fail("Tried to defer outside of a block");
		if (end - offset < u->getSize()) return cpp::fail("Tried to defer too big of a type");
		u64 element_offset = (offset - start) % element_type->getSize();
		if (!detail::typeAtOffset(element_type, element_offset, u))
			return cpp::fail("Type not present at offset");
		return base::ModRawView(data, u->getSize());
	}
}
