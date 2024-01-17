#pragma once

#include "../class_types.hpp"
#include "../kind.hpp"
#include "../type_desc.hpp"
#include "../type_desc.tcpp"
#include "../type_info.hpp"
#include "../types.hpp"
#include <base/string_id.hpp>
#include <base/smart_pointers.hpp>
#include <vector>

namespace ts::internal {
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes();

	template<typename T>
	void pushType(T type) {
		getTypes().emplace_back(base::unique_ptr<TypeInfoImpl>(std::move(type)));
	}

	class TypeInfoImpl {
	public:
		[[nodiscard]]
		virtual Kind getKind() const
			= 0;

		[[nodiscard]]
		usize getSize() const {
			return size;
		}

		[[nodiscard]]
		virtual const std::string& show() const {
			// @TODO: this is just a draft, in the future this method may
			// have verbosity / depth given as parameter
			return representation;
		}

		explicit TypeInfoImpl(usize size): size(size) {}

		[[nodiscard]]
		virtual bool isInfoImplicitlyCoercible([[maybe_unused]] const TypeInfo to) const {
			return false;
		}

		virtual ~TypeInfoImpl() = default;

	protected:
		// Size in bits.
		usize size = 0;
		// @TODO set this for each type and make it const.
		std::string representation = "UNNAMED";
	};

	class UnitInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Unit;
		}

		UnitInfoImpl(): TypeInfoImpl(0) { representation = "unit"; }
	};

	class VoidInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Void;
		}

		VoidInfoImpl(): TypeInfoImpl(0) { representation = "void"; }
	};

	class ByteInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Byte;
		}

		explicit ByteInfoImpl(): TypeInfoImpl(ts::BYTE_SIZE) { representation = "byte"; }

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// Implicit coercions allow checking against null bytes.
			return to.getKind() == Kind::Bool;
		}
	};

	class BoolInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Bool;
		}

		explicit BoolInfoImpl(): TypeInfoImpl(ts::BOOL_SIZE) { representation = "bool"; }

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// Implicit coercions allow adding to an integral counter.
			return to.getKind() == Kind::Integral;
		}
	};

	class CharInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Char;
		}

		explicit CharInfoImpl(): TypeInfoImpl(ts::CHAR_SIZE) { representation = "char"; }

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// Implicit coercions allow checking against null chars.
			return to.getKind() == Kind::Bool;
		}
	};

	class IntegralInfoImpl: public TypeInfoImpl {
		bool signedness;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Integral;
		}

		explicit IntegralInfoImpl(usize size, bool signedness):
			  TypeInfoImpl(size),
			  signedness(signedness) {
			if (signedness)
				representation = base::strConcat("int_", (u64) (size));
			else
				representation = base::strConcat("uint_", (u64) (size));
		}

		bool getSignedness() const { return signedness; }

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// Implicit coercions allow checking against zero,
			// as well as promoting to greater sizes and to floating point
			// numbers for physics simulations or similar
			return to.getKind() == Kind::Bool
			    || (to.getKind() == Kind::Integral && to.getSize() > size)
			    || to.getKind() == Kind::Float;
		}
	};

	class FloatInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Float;
		}

		explicit FloatInfoImpl(usize size): TypeInfoImpl(size) {
			representation = base::strConcat("float_", size);
		}

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// Implicit coercions allow promoting to greater sizes
			return to.getKind() == Kind::Float && FloatInfo(to).getSize() > size;
		}
	};

	class RawPointerInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::RawPointer;
		}

		RawPointerInfoImpl(): TypeInfoImpl(ts::POINTER_SIZE) { representation = "raw_pointer"; }

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// Implicit coercions allow checking against null pointer.
			// We do not allow casting to a typed pointer, because we forbid implicit type
			// specification.
			return to.getKind() == Kind::Bool;
		}
	};

	class PointerInfoImpl: public RawPointerInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Pointer;
		}

		[[nodiscard]]
		TypeDesc<> getUnderlying() const {
			return underlying_type;
		}

		explicit PointerInfoImpl(const TypeDesc<>& underlying_type):
			  RawPointerInfoImpl(),
			  underlying_type(underlying_type) {
			representation = base::strConcat("pointer(", underlying_type.getType().show(), ")");
		}

	protected:
		const TypeDesc<> underlying_type;
	};

	inline std::string showVector(const std::vector<TypeDesc<>>& types) {
		std::string res = "(";
		for (const auto& t: types) res += t.getType().show() + ",";
		res += ")";

		return res;
	}

	class FunctionInfoImpl: public TypeInfoImpl {
		std::vector<TypeDesc<>> parameterTypes;
		TypeDesc<>              resultType;
		base::FlagType          flags;  // like `pure` and others

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Function;
		}

		explicit FunctionInfoImpl(
			std::vector<TypeDesc<>> parameterTypes, TypeDesc<> resultType, i32 flags = 0
		):
			  TypeInfoImpl(POINTER_SIZE),
			  parameterTypes(std::move(parameterTypes)),
			  resultType(resultType),
			  flags(flags) {
			representation = "Function " + showVector(this->parameterTypes) + " -> ("
			               + resultType.getType().show() + ")";
		}

		[[nodiscard]]
		std::vector<TypeDesc<>> getParameterList() const {
			return parameterTypes;
		}

		[[nodiscard]]
		TypeDesc<> getResult() const {
			return resultType;
		}

		[[nodiscard]]
		base::FlagType getFlags() const {
			return flags;
		}

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			// A function type is convertible to another function type if and only if
			// the return type is coercible to the other return type and
			// the other parameter types are coercible to the parameter types,
			// similar to the rules of function subtyping.
			if (to.getKind() != Kind::Function) return false;
			const FunctionInfo toFunction = to;
			if (!flags.contains(toFunction.getFlags())
			    || parameterTypes.size() != toFunction.getParameterTypeList().size()) {
				return false;
			}
			for (usize i = 0; i < parameterTypes.size(); i++)
				if (!isImplicitlyCoercible(toFunction.getParameterTypeList()[i], parameterTypes[i]))
					return false;
			return isImplicitlyCoercible(resultType, toFunction.getResultType());
		}
	};

	class EnumInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Enum;
		}

		[[nodiscard]]
		IntegralInfo getBaseType() const {
			return base_type;
		}

		explicit EnumInfoImpl(IntegralInfo base_type):
			  TypeInfoImpl(base_type.getSize()),
			  base_type(base_type) {
			representation = "Enum " + base_type.show();
		}

	private:
		IntegralInfo base_type;
	};

	class FlagInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Flag;
		}

		[[nodiscard]]
		IntegralInfo getBaseType() const {
			return base_type;
		}

		explicit FlagInfoImpl(TypeInfo base_type):
			  TypeInfoImpl(base_type.getSize()),
			  base_type(base_type) {
			representation = "Flag " + base_type.show();
		}

	private:
		IntegralInfo base_type;
	};

	class OptionalInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Optional;
		}

		[[nodiscard]]
		TypeDesc<> getUnderlying() const {
			return underlying_type;
		}

		explicit OptionalInfoImpl(const TypeDesc<>& underlying_type):
			  TypeInfoImpl(BYTE_SIZE + underlying_type.getType().getSize()),
			  underlying_type(underlying_type) {
			"Optional " + underlying_type.getType().show();
		}

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			if (to.getKind() != Kind::Optional) return false;
			const OptionalInfo toOptional = to;
			return isImplicitlyCoercible(underlying_type, toOptional.getUnderlying());
		}

	protected:
		const TypeDesc<> underlying_type;
	};

	class TupleInfoImpl: public TypeInfoImpl {
		std::vector<TypeDesc<>> underlyingTypes;
		std::vector<usize>      offsets;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Tuple;
		}

		explicit TupleInfoImpl(const std::vector<TypeDesc<>>& underlyingTypes);

		const std::vector<TypeDesc<>>& getUnderlyingTypes() const { return underlyingTypes; }

		std::pair<TypeDesc<>, usize> getMember(usize index) const;

		TypeDesc<> getType(usize idx) const { return underlyingTypes[idx]; }

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			if (to.getKind() != Kind::Tuple) return false;
			const TupleInfo toTuple = to;
			if (underlyingTypes.size() != toTuple.getUnderlyingTypes().size()) return false;
			for (usize i = 0; i < underlyingTypes.size(); i++)
				if (!isImplicitlyCoercible(underlyingTypes[i], toTuple.getUnderlyingTypes()[i]))
					return false;
			return true;
		}
	};

	/** @TODO:
	 * Memory padding
	 * Dynamic "what am I?" information size based on input vector
	 * Sort variant types, so that var(A, B) = var(B, A)?
	 */
	class VariantInfoImpl: public TypeInfoImpl {
		std::vector<TypeDesc<>> variant_types;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Variant;
		}

		explicit VariantInfoImpl(const std::vector<TypeDesc<>>& variant_types);

		std::vector<TypeDesc<>> getTypes() const { return variant_types; }

		TypeDesc<> getType(usize idx) { return variant_types[idx]; }
	};

	class ClassInfoImpl: public TypeInfoImpl {
		const base::StrId name;

		// Stores the data of our ancestors, sorted by their offset
		std::vector<AncestorData> ancestors_data;
		// Stores the data of our members, could be sorted in the future
		std::vector<MemberData> members_data;

		// Indexes for looking up positions
		// If they contain something, it's not an empty vector
		base::Map<symtable::SymbolId, std::vector<usize>> members_positions;
		base::Map<ClassInfo, std::vector<usize>>          ancestors_positions;

		// Positions of our parents in the ancestors vector, sorted by their order
		std::vector<usize> basic_parents;
		// @TODO: they are a connected subsequence, maybe just remember first and last index in
		// members_data? Positions of our members in the members vector, sorted by their order
		std::vector<usize> direct_members;
		// Layout of virtual ancestors, sorted by their offset
		std::vector<std::pair<ClassInfo, usize>> virtual_layout;

		usize virt_method_count;
		usize base_size;

		[[nodiscard]]
		bool hasVtable() const;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Class;
		}

		[[nodiscard]]
		base::StrId getName() const {
			return name;
		}

		[[nodiscard]]
		const std::vector<AncestorData>& allAncestors() const;
		[[nodiscard]]
		const std::vector<AncestorData> basicParents() const;
		[[nodiscard]]
		const std::vector<ClassInfo> virtualAncestors() const;
		[[nodiscard]]
		const std::vector<MemberData> members() const;
		[[nodiscard]]
		const std::vector<MemberData>& allMembers() const;

		[[nodiscard]]
		MemberInfo getMemberInfo(symtable::SymbolId symbol) const;
		[[nodiscard]]
		MemberInfo getMemberInfo(symtable::SymbolId symbol, std::vector<ClassInfo> hint) const;

		[[nodiscard]]
		AncestorInfo getAncestorInfo(ClassInfo ancestor_id) const;
		[[nodiscard]]
		AncestorInfo getAncestorInfo(const std::vector<ClassInfo>& ancestor_ids) const;

		// Returns the offset of a single specific virtual ancestor
		[[nodiscard]]
		usize getVirtualAncestorOffset(ClassInfo ancestor_id) const;
		// Returns all the offsets of virtual members of the clueless parent, assuming it's been
		// created as a part of us. Since the virtual members are put at the end of the kid, they
		// will all be positive.
		[[nodiscard]]
		std::vector<std::pair<ClassInfo, usize>> getVirtualAncestorTable(ClassInfo ancestor_id
		) const;
		[[nodiscard]]
		std::vector<std::pair<ClassInfo, usize>>
			getVirtualAncestorTable(std::vector<ClassInfo> ancestor_ids) const;

		[[nodiscard]]
		usize getBaseSize() const;
		[[nodiscard]]
		usize getVtablePtrOffset() const;
		[[nodiscard]]
		usize getVtableSize() const;
		[[nodiscard]]
		usize getVtablePositionOf(ClassInfo ancestor) const;

		ClassInfoImpl(
			base::StrId                                                   name,
			const std::vector<std::pair<TypeDesc<>, symtable::SymbolId>>& member_types,
			const std::vector<std::pair<ClassInfo, InheritanceTag>>&      inheritance,
			usize                                                         virtualMethods
		);

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const override {
			if (to.getKind() != Kind::Class) return false;
			const ClassInfo toClass      = to;
			AncestorInfo    ancestorInfo = getAncestorInfo(toClass);
			return ancestorInfo.result_type == ResultType::Standard
			    || ancestorInfo.result_type == ResultType::Virtual;
		}
	};

	// Information how to bake the template into another type and what argument
	// lists were already baked is contained in the value of the template in exec.
	class TemplateInfoImpl: public TypeInfoImpl {
		// @TODO: Change to vector<TypeInfo> if TypeDescs end up not needed anywhere.
		std::vector<TypeDesc<>> parameter_list;

	public:
		explicit TemplateInfoImpl(const std::vector<TypeDesc<>>& parameter_list)
			  // @TODO: Change size to whatever StructTemplate or other value contained equals to.
			  :
			  TypeInfoImpl(0),
			  parameter_list(parameter_list) {
			// @TODO: this should have more information, probably name, and parameters
			representation = "Template";
		}

		const std::vector<TypeDesc<>>& getParameterList() const { return parameter_list; }
	};

	class TypeTemplateInfoImpl: public TemplateInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::TypeTemplate;
		}

		explicit TypeTemplateInfoImpl(const std::vector<TypeDesc<>>& parameter_list):
			  TemplateInfoImpl(parameter_list) {}
	};

	class NamespaceInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Namespace;
		}

		NamespaceInfoImpl(): TypeInfoImpl(0) {}
	};

	class CodeBlockInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::CodeBlock;
		}

		CodeBlockInfoImpl(): TypeInfoImpl(0) {}
	};

	class ModuleInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Module;
		}

		ModuleInfoImpl(): TypeInfoImpl(0) {}
	};

	class MetaInfoImpl: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::Meta;
		}

		explicit MetaInfoImpl(): TypeInfoImpl(ts::META_SIZE) {}
	};

	class VTableInfoImpl: public TypeInfoImpl {
		ClassInfo associated_class;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return Kind::VTable;
		}

		[[nodiscard]]
		ClassInfo getAssociatedClass() const;
		[[nodiscard]]
		usize getParentCount() const;
		[[nodiscard]]
		usize getMethodCount() const;

		explicit VTableInfoImpl(ClassInfo class_info):
			  TypeInfoImpl(class_info.getVtableSize() * sizeof(usize) * 8),
			  associated_class(class_info) {}
	};
}
