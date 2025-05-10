#pragma once

#include "../all.hpp"
#include "queries.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/simple.hpp>
#include <query_framework/context_fd.hpp>

#include <base/box.hpp>

#include <utility>
#include <vector>

namespace tsh::internal {
	/**
	 * @brief The AbstractTypeImpl class and its subclasses are a heavy type implementation
	 * hierarchy.
	 *
	 * An object from the AbstractTypeImpl hierarchy, like IntegralAbstractTypeImpl,
	 * FunctionAbstractTypeImpl etc. hold all the data describing a type (hence, they are heavy).
	 * This data includes:
	 * - the Kind of the type,
	 * - the size of a value of the type,
	 * - the TypeInterface of the type (fields and methods associated with the type), and
	 * - other kind-specific information, like
	 *   - signedness (for integral types), and
	 *   - member types (for tuples, functions, variants, and similar "composite" types).
	 *
	 * All methods of a AbstractTypeImpl subclass which should be accessible to the rest of the
	 * compiler must be forwarded in the corresponding AbstractType subclass.
	 *
	 * The AbstractTypeImpl hierarchy should not be included in any header files. It is the
	 * internal representation used by the Type System and should only be used in source code files
	 * of the Type System.
	 */
	class AbstractTypeImpl {
	public:
		/**
		 * @brief Gets the Kind of the type described by this object.
		 * @return The Kind of the type described by this object.
		 */
		[[nodiscard]]
		virtual Kind getKind() const
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
		virtual const TypeInterface& getInterface(query::Context& ctx) const {
			(void) ctx;
			static TypeInterface empty{};
			return empty;
		}

		/**
		 * @brief Get the text representation of this type.
		 * @return The text representation of this type.
		 */
		[[nodiscard]]
		const std::string& toString() const {
			// @TODO: this is just a draft, in the future this method may
			// have verbosity / depth given as parameter
			return representation;
		}

		/**
		 * @brief Determine whether it is legal to consider an implicit coercion
		 * from a value described by this ExpressionType to one described by target.
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
			[[maybe_unused]] const AbstractType target, [[maybe_unused]] query::Context& context
		) const {
			return false;
		}

		[[nodiscard]]
		AbstractType toAbstractType() const {
			return this;
		}

		virtual ~AbstractTypeImpl() = default;

	protected:
		// @TODO set this for each type and make it const.
		// @TODO make this a field in AbstractTypeImpl, set in the constructor?
		// Should be done when text representation for types is determined.
		/**
		 * @brief The text representation of this type.
		 */
		std::string representation = "UNNAMED";
	};

	std::vector<Box<const AbstractTypeImpl>>& getTypes();

	template<std::derived_from<AbstractTypeImpl> T>
	void pushType(Box<T>&& type) {
		getTypes().emplace_back(std::move(type));
	}

	class UnitAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Unit;

		UnitAbstractTypeImpl() { representation = "unit"; }
	};

	class VoidAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Void;

		VoidAbstractTypeImpl() { representation = "void"; }
	};

	class ByteAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Byte;

		explicit ByteAbstractTypeImpl() { representation = "byte"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow checking against null bytes.
			return target.getKind() == Kind::Bool;
		}
	};

	class BoolAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Bool;

		explicit BoolAbstractTypeImpl() { representation = "bool"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow adding to an integral counter.
			return target.getKind() == Kind::Integral;
		}
	};

	class CharAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Char;

		explicit CharAbstractTypeImpl() { representation = "char"; }

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow checking against null chars.
			return target.getKind() == Kind::Bool;
		}
	};

	class IntegralAbstractTypeImpl final: public AbstractTypeImpl {
		Bits size;
		bool signedness;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Integral;

		[[nodiscard]]
		Bits getSize() const {
			return size;
		}

		explicit IntegralAbstractTypeImpl(const usize size, const bool signedness):
			  size(Bits(size)),
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
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow checking against zero,
			// as well as promoting to greater sizes and to floating point
			// numbers for physics simulations or similar
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::Integral
			        && IntegralAbstractType(target).getSize() > size)
			    || target.getKind() == Kind::Float;
		}
	};

	class FloatAbstractTypeImpl final: public AbstractTypeImpl {
		Bits size;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Float;

		[[nodiscard]]
		Bits getSize() const {
			return size;
		}

		explicit FloatAbstractTypeImpl(usize size): size(Bits(size)) {
			representation = base::strConcat("f", size);
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow promoting to greater sizes
			return target.getKind() == Kind::Float && FloatAbstractType(target).getSize() > size;
		}
	};

	class RawPointerAbstractTypeImpl final: public AbstractTypeImpl {
		bool is_mutable;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::RawPointer;

		explicit RawPointerAbstractTypeImpl(const bool is_mutable): is_mutable(is_mutable) {
			representation = "raw_pointer";
		}

		[[nodiscard]]
		bool isMutable() const {
			return is_mutable;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow checking against null pointer and dropping mutability.
			// We do not allow casting to a typed pointer,
			// because we forbid implicit type specification in this context.
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::RawPointer
			        && (is_mutable || !RawPointerAbstractType(target).isMutable()));
		}
	};

	class PointerAbstractTypeImpl final: public AbstractTypeImpl {
		SymbolType<> pointee;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Pointer;

		[[nodiscard]]
		SymbolType<> getPointee() const {
			return pointee;
		}

		[[nodiscard]]
		AbstractType getUnderlyingType() const {
			return pointee.getType();
		}

		explicit PointerAbstractTypeImpl(const SymbolType<> component): pointee(component) {
			representation = base::strConcat("pointer(", component.toString(), ")");
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType target, query::Context& ctx) const override;
	};

	class StringAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::String;

		StringAbstractTypeImpl() { representation = "string"; }
	};

	class DynamicArrayAbstractTypeImpl final: public AbstractTypeImpl {
		SymbolType<> element_type;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::DynamicArray;

		DynamicArrayAbstractTypeImpl(const SymbolType<> element): element_type(element) {
			representation = base::strConcat("dynamic_array(", element.toString(), ")");
		}

		[[nodiscard]]
		SymbolType<> getElementType() const {
			return element_type;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType, query::Context&) const override {
			return false;
		}
	};

	class TupleAbstractTypeImpl final: public AbstractTypeImpl {
		std::vector<SymbolType<>> components;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Tuple;

		[[nodiscard]]
		const std::vector<SymbolType<>>& getComponents() const {
			return components;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType target, query::Context& ctx) const override;

		TupleAbstractTypeImpl(std::vector<SymbolType<>> components);
	};

	class FunctionAbstractTypeImpl final: public AbstractTypeImpl {
		std::vector<SymbolType<>> parameter_types;
		SymbolType<>              result_type;
		bool                      pure, free;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Function;

		[[nodiscard]]
		const std::vector<SymbolType<>>& getParameterTypes() const {
			return parameter_types;
		}

		[[nodiscard]]
		SymbolType<> getResult() const {
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
		bool isImplicitlyCoercible(AbstractType target, query::Context& context) const override;

		FunctionAbstractTypeImpl(
			std::vector<SymbolType<>> parameter_types,
			SymbolType<>              result_type,
			bool                      pure = false,
			bool                      free = false
		);
	};

	/** @TODO:
	 * Memory padding
	 * Dynamic "what am I?" information size based on input vector
	 * Sort variant types, so that var(A, B) = var(B, A)?
	 */
	class VariantAbstractTypeImpl final: public AbstractTypeImpl {
		std::vector<SymbolType<>> underlying_types;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Variant;

		explicit VariantAbstractTypeImpl(const std::vector<SymbolType<>>& variant_types);

		[[nodiscard]]
		const std::vector<SymbolType<>>& getUnderlyingTypes() const {
			return underlying_types;
		}

		[[nodiscard]]
		SymbolType<> getMember(const usize idx) const {
			return underlying_types[idx];
		}
	};

	class ClassAbstractTypeImpl final: public AbstractTypeImpl {
		compiler::helios::SymID symbol;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Class;

		[[nodiscard]]
		const TypeInterface& getInterface(query::Context& ctx) const override;

		explicit ClassAbstractTypeImpl(compiler::helios::SymID symbol);

		[[nodiscard]]
		compiler::helios::SymID getSymbol() const {
			return symbol;
		}

		[[nodiscard]]
		base::Optional<ClassAbstractType> getBaseClassType(query::Context& ctx) const;

		[[nodiscard]]
		base::Optional<compiler::helios::SymID> getBaseClassSymbol(query::Context& ctx) const {
			return getBaseClassType(ctx).map([](ClassAbstractType class_info) {
				return class_info.getSymbol();
			});
		}

		// @TODO: change return type to InterfaceInfo when interface type is created.
		[[nodiscard]]
		std::vector<ClassAbstractType> getImplementedInterfaceTypes(query::Context& ctx) const;

		[[nodiscard]]
		std::vector<compiler::helios::SymID> getImplementedInterfaceSymbols(query::Context& ctx
		) const;

		[[nodiscard]]
		SymbolType<> getMemberType(compiler::helios::SymID sym, query::Context& ctx) const {
			const auto& elements_with_same_name = getInterface(ctx).getElements().at(name(sym));
			for (const auto& element: elements_with_same_name)
				if (element.getSymbol() == sym) return element.getType(ctx);
			CORE_PANIC("Element not found.");
		}
	};

	class NamespaceAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Namespace;

		NamespaceAbstractTypeImpl() = default;
	};

	class ModuleAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Module;

		ModuleAbstractTypeImpl() = default;
	};

	class MetaAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Meta;

		explicit MetaAbstractTypeImpl() { representation = "META"; }
	};

	class ImportAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Import;

		explicit ImportAbstractTypeImpl() = default;
	};
}
