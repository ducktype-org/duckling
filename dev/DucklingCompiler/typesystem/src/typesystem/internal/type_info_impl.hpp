#pragma once

#include <base/string_id.hpp>
#include <base/smart_pointers.hpp>
#include <utility>
#include <vector>

#include <query_framework/query_impl.hpp>

#include "../typesystem.hpp"
#include "queries.hpp"

#include <helios/symbols/symbols.hpp>

namespace ts::internal {
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes();

	template<std::derived_from<TypeInfoImpl> T>
	void pushType(base::unique_ptr<T>&& type) {
		getTypes().emplace_back(base::unique_ptr<TypeInfoImpl>(std::move(type)));
	}

	/**
	 * @brief The TypeInfoImpl class and its subclasses are a heavy type implementation hierarchy.
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
		 * @brief The Kind of the type described by an object of this class.
		 */
		static constexpr Kind staticKind = Kind::Any;

		/**
		 * @brief Gets the Kind of the type described by this object.
		 * @return The Kind of the type described by this object.
		 */
		[[nodiscard]]
		virtual Kind getKind() const
			= 0;

		/**
		 * @brief Gets the size of a value of the type described by this object, in bits.
		 * @param ctx The Query Context necessary to deduce composite type sizes.
		 * This is applicable for types which require being at some point "incomplete".
		 * @return The size of a value of the type described by this object, in bits.
		 */
		[[nodiscard]]
		virtual usize getSize(query::Context& ctx) const
			= 0;

		/**
		 * @brief Gets the TypeInterface of the type described by this class.
		 * @param ctx The Query Context necessary to deduce interfaces.
		 * This is applicable for types which require being at some point "incomplete".
		 * @return The TypeInterface of the type described by this class.
		 */
		// @TODO: Remove the default for the interface. Each type should know its interface.
		// The interface default is to be removed when interfaces for each type are determined.
		// Then, this definition should become pure virtual.
		[[nodiscard]]
		virtual TypeInterface& getInterface(query::Context&) const {
			static TypeInterface empty{};
			return empty;
		}

		/**
		 * @brief Get the text representation of this type.
		 * @return The text representation of this type.
		 */
		[[nodiscard]]
		virtual const std::string& toString() const {
			// @TODO: this is just a draft, in the future this method may
			// have verbosity / depth given as parameter
			return representation;
		}

		/**
		 * @brief Determine whether it is legal to consider an implicit coercion
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
		 * @param target The target of a hypothetical implicit coercion.
		 * @param context The query context necessary for checking user defined coercions.
		 * @return Whether the implicit coercion is allowed or not.
		 */
		[[nodiscard]]
		virtual bool isImplicitlyCoercible(
			[[maybe_unused]] const TypeInfo target, [[maybe_unused]] query::Context& context
		) const {
			return false;
		}

		virtual ~TypeInfoImpl() = default;

	protected:
		// @TODO set this for each type and make it const.
		// @TODO make this a field in TypeInfoImpl, set in the constructor?
		// Should be done when text representation for types is determined.
		/**
		 * @brief The text representation of this type.
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Unit;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return 0;
		}

		UnitInfoImpl() { representation = "unit"; }
	};

	class VoidInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Void;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return 0;
		}

		VoidInfoImpl() { representation = "void"; }
	};

	class ByteInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Byte;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return BYTE_SIZE;
		}

		explicit ByteInfoImpl() { representation = "byte"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context&) const override {
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Bool;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return BOOL_SIZE;
		}

		explicit BoolInfoImpl() { representation = "bool"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context&) const override {
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Char;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return CHAR_SIZE;
		}

		explicit CharInfoImpl() { representation = "char"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context&) const override {
			// Implicit coercions allow checking against null chars.
			return target.getKind() == Kind::Bool;
		}
	};

	class IntegralInfoImpl final: public TypeInfoImpl {
		usize size;
		bool  signedness;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Integral;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return size;
		}

		explicit IntegralInfoImpl(const usize size, const bool signedness):
			  size(size),
			  signedness(signedness) {
			if (signedness)
				representation = base::strConcat("i", size);
			else
				representation = base::strConcat("u", size);
		}

		[[nodiscard]]
		bool getSignedness() const {
			return signedness;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context& ctx) const override {
			// Implicit coercions allow checking against zero,
			// as well as promoting to greater sizes and to floating point
			// numbers for physics simulations or similar
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::Integral && target.getSize(ctx) > size)
			    || target.getKind() == Kind::Float;
		}
	};

	class FloatInfoImpl final: public TypeInfoImpl {
		usize size;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Float;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return size;
		}

		explicit FloatInfoImpl(usize size): size(size) {
			representation = base::strConcat("f", size);
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context& ctx) const override {
			// Implicit coercions allow promoting to greater sizes
			return target.getKind() == Kind::Float && FloatInfo(target).getSize(ctx) > size;
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::RawPointer;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return POINTER_SIZE;
		}

		explicit RawPointerInfoImpl(const bool is_mutable): is_mutable(is_mutable) {
			representation = "raw_pointer";
		}

		[[nodiscard]]
		bool isMutable() const {
			return is_mutable;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context&) const override {
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Pointer;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return POINTER_SIZE;
		}

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

		explicit PointerInfoImpl(const ComponentType component): component(component) {
			representation = base::strConcat(
				"pointer(", component.is_mutable ? "" : "const", component.type.toString(), ")"
			);
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context&) const override {
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Reference;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return POINTER_SIZE;
		}

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
			  underlying_type(underlying_type),
			  ref_kind(ref_kind),
			  leaking(leaking),
			  nullable(nullable),
			  unique(unique) {}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo, query::Context&) const override {
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Tuple;

		[[nodiscard]]
		usize getSize(query::Context& ctx) const override {
			return ctx.query<QuerySizeOfTuple>({ this });
		}

		[[nodiscard]]
		const std::vector<ComponentType>& getComponents() const {
			return components;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const TypeInfo target, query::Context& ctx) const override {
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
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Function;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return (1 + !free) * POINTER_SIZE;
		}

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
		bool isImplicitlyCoercible(const TypeInfo target, query::Context& context) const override {
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
		std::vector<TypeInfo> underlying_types;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Variant;

		[[nodiscard]]
		usize getSize(query::Context& ctx) const override {
			return ctx.query<QuerySizeOfVariant>({ this });
		}

		explicit VariantInfoImpl(const std::vector<TypeInfo>& variant_types);

		[[nodiscard]]
		const std::vector<TypeInfo>& getUnderlyingTypes() const {
			return underlying_types;
		}

		[[nodiscard]]
		TypeInfo getMember(const usize idx) const {
			return underlying_types[idx];
		}
	};

	class ClassInfoImpl final: public TypeInfoImpl {
		compiler::helios::SymID symbol;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static inline Kind staticKind = Kind::Class;

		[[nodiscard]]
		usize getSize(query::Context& ctx) const override {
			return ctx.query<QuerySizeOfClass>({ this });
		}

		explicit ClassInfoImpl(compiler::helios::SymID symbol);

		[[nodiscard]]
		compiler::helios::SymID getSymbol() const {
			return symbol;
		}

		[[nodiscard]]
		base::Optional<ClassInfo> getBaseClassType(query::Context& ctx) const {
			auto& bases = ctx.query<compiler::helios::QueryStructSymbolData>(symbol).bases;
			if (bases.empty())
				return {};
			else
				return { ClassInfo(bases.at(0)) };
		}

		[[nodiscard]]
		base::Optional<compiler::helios::SymID> getBaseClassSymbol(query::Context& ctx) const {
			return getBaseClassType(ctx).map([](ClassInfo classInfo) {
				return classInfo.getSymbol();
			});
		}

		// @TODO: change return type to InterfaceInfo when interface type is created.
		[[nodiscard]]
		std::vector<ClassInfo> getImplementedInterfaceTypes(query::Context& ctx) const {
			auto& bases = ctx.query<compiler::helios::QueryStructSymbolData>(symbol).bases;
			std::vector<ClassInfo> result;
			result.reserve(std::max(0UL, bases.size() - 1));
			for (int i = 1; i < bases.size(); i++) result.emplace_back(bases.at(i));
			return result;
		}

		[[nodiscard]]
		std::vector<compiler::helios::SymID> getImplementedInterfaceSymbols(query::Context& ctx
		) const {
			auto& bases = ctx.query<compiler::helios::QueryStructSymbolData>(symbol).bases;
			std::vector<compiler::helios::SymID> result;
			result.reserve(std::max(0UL, bases.size() - 1));
			for (int i = 1; i < bases.size(); i++)
				// @TODO: change cast type to InterfaceInfo when interface type is created.
				result.push_back(ClassInfo(bases.at(i)).getSymbol());
			return result;
		}

		[[nodiscard]]
		TypeInfo getMemberType(compiler::helios::SymID sym, query::Context& ctx) const {
			const auto& elements_with_same_name = getInterface(ctx).getElements().at(name(sym));
			for (const auto& element: elements_with_same_name)
				if (element.getSymbol() == sym) return element.getType(ctx);
			DUCKLING_PANIC("Element not found.");
		}
	};

	class NamespaceInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Namespace;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return 0;
		}

		NamespaceInfoImpl() = default;
	};

	class ModuleInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Module;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return 0;
		}

		ModuleInfoImpl() = default;
	};

	class MetaInfoImpl final: public TypeInfoImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return staticKind;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind staticKind = Kind::Meta;

		[[nodiscard]]
		usize getSize(query::Context&) const override {
			return META_SIZE;
		}

		explicit MetaInfoImpl() = default;
	};
}
