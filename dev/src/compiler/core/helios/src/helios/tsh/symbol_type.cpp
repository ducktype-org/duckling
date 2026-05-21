#include "symbol_type.hpp"

#include "helios/tsh/abstract_type.hpp"
#include "helios/tsh/types.hpp"

namespace compiler::tsh {

	template<std::derived_from<AbstractType> ABSTRACT_TYPE>
	SymbolType<ABSTRACT_TYPE> SymbolType<ABSTRACT_TYPE>::getPointeeSymbolType() const {
		// If this is Ref or Box we want to get the pointee type.
		if (reference_kind == ReferenceKind::Ref || reference_kind == ReferenceKind::Box)
			return withReferenceKind(ReferenceKind::Direct);
		// Not this has to be a pointer
		auto get_pointee_type = [&](tsh::AbstractType abstract_type) -> tsh::SymbolType<> {
			switch (abstract_type.getKind()) {
			case tsh::Kind::Pointer:
				return abstract_type.as<tsh::PointerAbstractType>().getPointee();
			case tsh::Kind::ManyPointer:
				return abstract_type.as<tsh::ManyPointerAbstractType>().getPointee();
			case tsh::Kind::CPointer:
				return abstract_type.as<tsh::CPointerAbstractType>().getPointee();
			default:
				CORE_PANIC("Cannot dereference a non-pointer like type");
			}
		};
		return get_pointee_type(abstract_type);
	}

	template auto SymbolType<AbstractType>::getPointeeSymbolType() const -> SymbolType<AbstractType>;
}
