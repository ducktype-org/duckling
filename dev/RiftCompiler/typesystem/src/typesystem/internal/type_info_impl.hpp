#pragma once

#include "../kind.hpp"
#include "../type_desc.hpp"
#include "../type_info.hpp"
#include "../types.hpp"
#include "../type_interface.hpp"
#include "../queries/implicit_coercibility.hpp"
#include <base/string_id.hpp>
#include <base/smart_pointers.hpp>
#include <utility>
#include <vector>
#include <query_framework/query_impl.hpp>

namespace ts::internal {
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes();

	template<std::derived_from<TypeInfoImpl> T>
	void pushType(base::unique_ptr<T>&& type) {
		getTypes().emplace_back(base::unique_ptr<TypeInfoImpl>(std::move(type)));
	}

	/**
	 * \brief The TypeInfoImpl class and its subclasses are a heavy type implementation hierarchy.
	 *
	 * An object from the TypeInfoImpl hierarchy, like IntegralInfoImpl, FunctionInfoImpl etc. hold
	 * all the data describing a type (hence, they are heavy). This data includes:
	 * - the Kind of the type,
	 * - the size of a value of the type,
	 * - the TypeInterface of the type (fields and methods associated with the type), and
	 * - other kind-specific information, like
	 *   - signedness (for integral types), and
	 *   - member types (for tuples, functions, variants, and similar "composite" types).
	 *
	 * All methods of a TypeInfoImpl subclass which should be accessible to the rest of the compiler
	 * must be forwarded in the corresponding TypeInfo subclass.
	 *
	 * The TypeInfoImpl hierarchy should not be included in any header files. It is the internal
	 * representation used by the Type System and should only be used in source code files of the
	 * Type System.
	 */
	class TypeInfoImpl {
	public:
		/**
		 * \brief The Kind of the type described by an object of this class.
		 */
		static Kind staticKind;

		/**
		 * \brief Gets the Kind of the type described by this object.
		 * \return The Kind of the type described by this object.
		 */
		[[nodiscard]]
		virtual Kind getKind() const
			= 0;

		/**
		 * \brief Gets the size of a value of the type described by this object, in bits.
		 * \return The size of a value of the type described by this object, in bits.
		 */
		[[nodiscard]]
		usize getSize() const {
			return size;
		}

		/**
		 * \brief Gets the TypeInterface of the type described by this class.
		 * \return The TypeInterface of the type described by this class.
		 */
		[[nodiscard]]
		TypeInterface getInterface() const {
			return interface;
		}

		/**
		 * \brief Get the text representation of this type.
		 * \return The text representation of this type.
		 */
		[[nodiscard]]
		virtual const std::string& toString() const {
			// @TODO: this is just a draft, in the future this method may
			// have verbosity / depth given as parameter
			return representation;
		}

		/**
		 * \brief Construct a type, given its size and interface.
		 *
		 * This constructor should be used when everything about a type is known,
		 * i.e. when the type is complete.
		 * \param size The size of the type.
		 * \param interface The interface of the type.
		 */
		// @TODO: remove the default for the interface. Each type should know its interface.
		// The interface default is to be removed when interfaces for each type are determined.
		explicit TypeInfoImpl(const usize size, TypeInterface interface = {}):
			  size(size),
			  interface(std::move(interface)) {}

		/**
		 * \brief Determine whether it is legal to consider an implicit coercion
		 * from a value described by this TypeDesc to one described by target.
		 *
		 * An implicit coercion is when, for example, a boolean is expected, but
		 * and integer is given. A desirable (and common) behaviour may be to
		 * convert the integer value to true if and only if it is non-zero.
		 *
		 * Another context in which implicit coercions are desirable is when
		 * casting from subclass to superclass.
		 *
		 * This method does not determine how to perform a coercion.
		 * It only determines whether one should be considered.
		 * A coercion may thus be allowed but not implemented, or implemented
		 * but not allowed to be used implicitly by the compiler, so the user
		 * may define a coercion from class A to class B, but not want it
		 * to ever be used implicitly (in C++ that is achieved by annotating a
		 * single-argument constructor with the `explicit` keyword).
		 *
		 * This is typically determined by rules specific for the Kind of the source type.
		 *
		 * \param target The target of a hypothetical implicit coercion.
		 * \param context The query context necessary for checking user defined coercions.
		 * \return Whether the implicit coercion is allowed or not.
		 */
		[[nodiscard]]
		virtual bool isImplicitlyCoercible(
			[[maybe_unused]] const TypeInfo              target,
			[[maybe_unused]] query::detail::ContextType& context
		) const {
			return false;
		}

		virtual ~TypeInfoImpl() = default;

	protected:
		/**
		 * \brief Size in bits.
		 */
		usize size = 0;

		/**
		 * \brief The interface of the type described by an object of this class.
		 */
		const TypeInterface interface;

		// @TODO set this for each type and make it const.
		// @TODO make this a field in TypeInfoImpl, set in the constructor?
		// Should be done when text representation for types is determined.
		/**
		 * \brief The text representation of this type.
		 */
		std::string representation = "UNNAMED";
	};

	class UnitInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		UnitInfoImpl(): TypeInfoImpl(0) { representation = "unit"; }
	};

	class VoidInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		VoidInfoImpl(): TypeInfoImpl(0) { representation = "void"; }
	};

	class ByteInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit ByteInfoImpl(): TypeInfoImpl(BYTE_SIZE) { representation = "byte"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Implicit coercions allow checking against null bytes.
			return target.getKind() == Kind::Bool;
		}
	};

	class BoolInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit BoolInfoImpl(): TypeInfoImpl(BOOL_SIZE) { representation = "bool"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Implicit coercions allow adding to an integral counter.
			return target.getKind() == Kind::Integral;
		}
	};

	class CharInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit CharInfoImpl(): TypeInfoImpl(CHAR_SIZE) { representation = "char"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Implicit coercions allow checking against null chars.
			return target.getKind() == Kind::Bool;
		}
	};

	class IntegralInfoImpl final: public TypeInfoImpl {
		bool signedness;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit IntegralInfoImpl(const usize size, const bool signedness):
			  TypeInfoImpl(size),
			  signedness(signedness) {
			if (signedness)
				representation = base::strConcat("int_", size);
			else
				representation = base::strConcat("uint_", size);
		}

		[[nodiscard]]
		bool getSignedness() const {
			return signedness;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Implicit coercions allow checking against zero,
			// as well as promoting to greater sizes and to floating point
			// numbers for physics simulations or similar
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::Integral && target.getSize() > size)
			    || target.getKind() == Kind::Float;
		}
	};

	class FloatInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit FloatInfoImpl(usize size): TypeInfoImpl(size) {
			representation = base::strConcat("float_", size);
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Implicit coercions allow promoting to greater sizes
			return target.getKind() == Kind::Float && FloatInfo(target).getSize() > size;
		}
	};

	class RawPointerInfoImpl final: public TypeInfoImpl {
		const bool is_mutable;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit RawPointerInfoImpl(const bool is_mutable):
			  TypeInfoImpl(POINTER_SIZE),
			  is_mutable(is_mutable) {
			representation = "raw_pointer";
		}

		[[nodiscard]]
		bool isMutable() const {
			return is_mutable;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Implicit coercions allow checking against null pointer and dropping mutability.
			// We do not allow casting to a typed pointer,
			// because we forbid implicit type specification in this context.
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::RawPointer
			        && (is_mutable || !RawPointerInfo(target).isMutable()));
		}
	};

	class PointerInfoImpl final: public TypeInfoImpl {
		ComponentType component;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		[[nodiscard]]
		ComponentType getComponent() const {
			return component;
		}

		[[nodiscard]]
		TypeInfo getUnderlyingType() const {
			return component.type;
		}

		[[nodiscard]]
		bool isMutable() const {
			return component.is_mutable;
		}

		explicit PointerInfoImpl(const ComponentType component):
			  TypeInfoImpl(POINTER_SIZE),
			  component(component) {
			representation = base::strConcat(
				"pointer(", component.is_mutable ? "" : "const", component.type.toString(), ")"
			);
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType&)
			const override {
			// Explicit override without change in implementation to add comment.
			// Implicit coercions allow checking against null pointer.
			// We do not allow casting to another (raw) pointer type,
			// because we forbid implicit type (de)specification in this context.
			// We only allow dropping mutability.
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::Pointer
			        && (isMutable() || !PointerInfo(target).isMutable()));
		}
	};

	class ReferenceInfoImpl final: public TypeInfoImpl {
		const TypeInfo      underlying_type;
		const ReferenceKind ref_kind;
		const bool          leaking, nullable, unique;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		[[nodiscard]]
		TypeInfo getUnderlyingType() const {
			return underlying_type;
		}

		[[nodiscard]]
		ReferenceKind getReferenceKind() const {
			return ref_kind;
		}

		[[nodiscard]]
		bool isLeaking() const {
			return leaking;
		}

		[[nodiscard]]
		bool isNullable() const {
			return nullable;
		}

		[[nodiscard]]
		bool isUnique() const {
			return unique;
		}

		explicit ReferenceInfoImpl(
			const TypeInfo      underlying_type,
			const ReferenceKind ref_kind,
			const bool          leaking,
			const bool          nullable,
			const bool          unique
		):
			  TypeInfoImpl(POINTER_SIZE),
			  underlying_type(underlying_type),
			  ref_kind(ref_kind),
			  leaking(leaking),
			  nullable(nullable),
			  unique(unique) {}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo, query::detail::ContextType&) const override {
			// Unlike with pointers, we do not allow checking whether the reference is non-null by
			// coercion, because this may conflict with the underlying type being coercible to bool.
			// Instead, we would want to just forward coercibility.
			// But we also anticipate the need to coerce `T` to `ref T` or the other way around.
			// Does this mean that we need to coerce `(ref S)` to `ref (ref S)`?
			// Does `ref ref S` even make sense?
			// @TODO: resolve the above.
			return false;
		}
	};

	class TupleInfoImpl final: public TypeInfoImpl {
		const std::vector<ComponentType> components;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		[[nodiscard]]
		const std::vector<ComponentType>& getComponents() const {
			return components;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType& ctx)
			const override {
			// Implicit coercions are allowed to other tuples of the same size,
			// where each component can be coerced independently.

			if (target.getKind() != Kind::Tuple) return false;
			TupleInfo targetTuple = target;

			const std::vector<ComponentType>& targetComponents = targetTuple.getComponents();
			if (targetComponents.size() != components.size()) return false;

			for (usize i = 0; i < components.size(); i++) {
				ComponentType component       = components[i];
				ComponentType targetComponent = targetComponents[i];
				if (!component.isImplicitlyCoercible(targetComponent, ctx)) return false;
			}

			return true;
		}

		TupleInfoImpl(std::vector<ComponentType> components);
	};

	class FunctionInfoImpl final: public TypeInfoImpl {
		const std::vector<TypeInfo> parameter_types;
		const TypeInfo              result_type;
		const bool                  pure, free;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		[[nodiscard]]
		const std::vector<TypeInfo>& getParameterTypes() const {
			return parameter_types;
		}

		[[nodiscard]]
		TypeInfo getResult() const {
			return result_type;
		}

		[[nodiscard]]
		bool isPure() const {
			return pure;
		}

		[[nodiscard]]
		bool isFree() const {
			return free;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::detail::ContextType& context)
			const override {
			// A function type is coercible to another function type if and only if
			// the return type is coercible to the other return type and
			// the other parameter types are coercible to the parameter types,
			// similar to the rules of function subtyping.
			//
			// Additionally, only a pure function can be coerced to a pure function,
			// and only a free function can be coerced to a free function.

			if (target.getKind() != Kind::Function) return false;
			const FunctionInfo targetFunction = target;
			if ((!pure && targetFunction.isPure()) || (!free && targetFunction.isFree())
			    || parameter_types.size() != targetFunction.getParameterTypes().size()) {
				return false;
			}

			for (usize i = 0; i < parameter_types.size(); i++)
				if (!context.query<QueryImplicitCoercibilityOnInfo>({
						targetFunction.getParameterTypes()[i],
						parameter_types[i],
					}))
					return false;
			return context.query<QueryImplicitCoercibilityOnInfo>({
				result_type,
				targetFunction.getResultType(),
			});
		}

		FunctionInfoImpl(
			std::vector<TypeInfo> parameter_types,
			TypeInfo              result_type,
			bool                  pure = false,
			bool                  free = false
		);
	};

	/** @TODO:
	 * Memory padding
	 * Dynamic "what am I?" information size based on input vector
	 * Sort variant types, so that var(A, B) = var(B, A)?
	 */
	class VariantInfoImpl final: public TypeInfoImpl {
		std::vector<TypeDesc<>> underlyingTypes;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit VariantInfoImpl(const std::vector<TypeDesc<>>& variant_types);

		[[nodiscard]]
		const std::vector<TypeDesc<>>& getUnderlyingTypes() const {
			return underlyingTypes;
		}

		[[nodiscard]]
		TypeDesc<> getMember(const usize idx) const {
			return underlyingTypes[idx];
		}
	};

	class NamespaceInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		NamespaceInfoImpl(): TypeInfoImpl(0) {}
	};

	class ModuleInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		ModuleInfoImpl(): TypeInfoImpl(0) {}
	};

	class MetaInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * \brief The Kind of types described by objects of this class.
		 */
		static Kind staticKind;

		explicit MetaInfoImpl(): TypeInfoImpl(META_SIZE) {}
	};
}
