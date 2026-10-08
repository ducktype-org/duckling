// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/kind.hpp>
#include <helios/tsh/mutability.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <query_framework/context/context_fd.hpp>

#include <utility>
#include <vector>

namespace compiler::helios {
	// Forwards:
	struct ClassSymbolData;
}

namespace compiler::tsh {
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
	 * private representation used by the Type System and should only be used in source code files
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
		 * Get the complete interface of the type (including default methods and user-defined ones).
		 * @note Uses an internal query for caching.
		 */
		[[nodiscard]]
		CRef<TypeInterface> getInterface(query::Context& ctx) const;

		/**
		 * Same as @ref getInterface, but gives the caller the failure instead of throwing it, for
		 * the queries that cannot let a query-failure exception escape their `provide`.
		 */
		[[nodiscard]]
		CRef<query::QResult<TypeInterface>> getInterfaceResult(query::Context& ctx) const;

		/**
		 * @brief Check if the type is a simple type, which correlates heavily with the type being
		 * more efficient to be passed by copy instead of by reference.
		 * @return Whether the type is a simple type.
		 */
		[[nodiscard]]
		bool isSimple() const;

		/**
		 * @brief Determines weather the type has a trivial destructor.
		 * For more details look in `symbol_type.hpp`.
		 *
		 * @return true if the type has a trivial destructor, false otherwise.
		 */
		[[nodiscard]]
		virtual bool isTriviallyDestructible(query::Context& ctx) const
			= 0;

		/**
		 * @brief Determines weather the type has a default constructor.
		 * For more details look in `symbol_type.hpp`.
		 *
		 * @return true if the type has a default constructor, false otherwise.
		 */
		[[nodiscard]]
		virtual bool isDefaultConstructible(query::Context& ctx) const
			= 0;

		/**
		 * @brief Determines weather the type has a trivial zero constructor.
		 * For more details look in `symbol_type.hpp`.
		 *
		 * @return true if the type can be default initialized by zeros, false otherwise
		 */
		[[nodiscard]]
		virtual bool isTriviallyZeroInitializable(query::Context& ctx) const
			= 0;

		/**
		 * @brief Checks if a value of this type can be copied.
		 * For more details look in `symbol_type.hpp`.
		 *
		 * @return True if the type is copyable, false otherwise.
		 */
		[[nodiscard]]
		virtual bool isCopyable(query::Context& ctx) const
			= 0;

		/**
		 * @brief Checks if a value of this type can be copied trivially by just copying the values
		 * bytes.
		 * For more details look in `symbol_type.hpp`.
		 *
		 * @return True if the symbol is trivially copyable, false otherwise.
		 */
		[[nodiscard]]
		virtual bool isTriviallyCopyable(query::Context& ctx) const
			= 0;

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

		/**
		 * @brief Whether the type carries any information, in an information-theoretic sense. For
		 * example, the unit and void types does not carry any information, while other types do.
		 * @param ctx Query context needed to process complex types, esp. classes.
		 * @return Whether the type carries information.
		 */
		[[nodiscard]]
		virtual bool carriesInformation(query::Context&) const {
			return true;
		}

		[[nodiscard]]
		AbstractType toAbstractType() const {
			return CRef(this);
		}

		virtual ~AbstractTypeImpl() = default;

	protected:
		/**
		 * @brief Gets the TypeInterface of the type described by this class, excluding elements
		 * which belong to the default interface for each type. In other words, this is the part of
		 * the interface which has been declared by the user.
		 * @param ctx The Query Context necessary to deduce interfaces.
		 * @return The TypeInterface of the type described by this class.
		 */
		[[nodiscard]]
		virtual CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const
			= 0;

		friend struct ImplementationOf_QueryTypeInterface;

		// @TODO set this for each type and make it const.
		// @TODO make this a field in AbstractTypeImpl, set in the constructor?
		// Should be done when text representation for types is determined.
		/**
		 * @brief The text representation of this type.
		 */
		std::string representation = "UNNAMED";
	};

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

		UnitAbstractTypeImpl() { representation = "()"; }

		[[nodiscard]] bool isImplicitlyCoercible(AbstractType target, query::Context& context)
			const override;

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool carriesInformation(query::Context&) const override { return false; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType, query::Context&) const override {
			// Void has no values, so it is a subtype of every type.
			return true;
		}

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return false; }

		[[nodiscard]] bool carriesInformation(query::Context&) const override { return false; }

		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	class IntegralAbstractTypeImpl final: public AbstractTypeImpl {
		Bits                             size;
		IntegralAbstractType::Signedness signedness;

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

		explicit IntegralAbstractTypeImpl(
			const usize size, const IntegralAbstractType::Signedness signedness
		):
			  size(Bits(size)),
			  signedness(signedness) {
			if (signedness == IntegralAbstractType::Signedness::Signed)
				representation = base::strConcat("i", size);
			else
				representation = base::strConcat("u", size);
		}

		[[nodiscard]]
		IntegralAbstractType::Signedness getSignedness() const {
			return signedness;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Only allow promoting to greater sizes
			// signed to unsigned coercions are not allowed
			// @TODO: #3631 we may bring back coercions to bool in the future, but that requires
			// solving an issue with overload resolution, e.g. with `1u64 == 2i64`.
			auto int_coercion = (target.getKind() == Kind::Integral);
			auto upsize_coercion
				= (int_coercion && (IntegralAbstractType(target).getSize() > size));
			auto drop_sign_coercion
				= (int_coercion && signedness == IntegralAbstractType::Signedness::Signed
			       && IntegralAbstractType(target).getSignedness()
			              == IntegralAbstractType::Signedness::Unsigned);

			return upsize_coercion && !drop_sign_coercion;
		}

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	class RawPointerAbstractTypeImpl final: public AbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::RawPointer;

		explicit RawPointerAbstractTypeImpl(const Mutability mutability): mutability(mutability) {
			representation = "raw_pointer";
		}

		[[nodiscard]]
		bool isMutable() const {
			return mutability == Mutability::Mutable;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(const AbstractType target, query::Context&) const override {
			// Implicit coercions allow checking against null pointer and dropping mutability.
			// We do not allow casting to a typed pointer,
			// because we forbid implicit type specification in this context.
			return target.getKind() == Kind::Bool
			    || (target.getKind() == Kind::RawPointer
			        && (mutability == Mutability::Mutable
			            || !RawPointerAbstractType(target).isMutable()));
		}

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;

	private:
		Mutability mutability;
	};

	class PointerAbstractTypeImpl: public AbstractTypeImpl {
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
			representation = base::strConcat("ptr ", component.toString());
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType target, query::Context& ctx) const override;

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return true;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	class ManyPointerAbstractTypeImpl final: public PointerAbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::ManyPointer;

		explicit ManyPointerAbstractTypeImpl(const SymbolType<> component):
			  PointerAbstractTypeImpl(component) {
			representation = base::strConcat("manyptr ", component.toString());
		}

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	class CPointerAbstractTypeImpl final: public PointerAbstractTypeImpl {
	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::CPointer;

		explicit CPointerAbstractTypeImpl(const SymbolType<> component):
			  PointerAbstractTypeImpl(component) {
			representation = base::strConcat("cptr ", component.toString());
		}

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	class SliceAbstractTypeImpl final: public AbstractTypeImpl {
		SymbolType<> element;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		[[nodiscard]]
		SymbolType<> getElementType() const {
			return element;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Slice;

		explicit SliceAbstractTypeImpl(const SymbolType<> element): element(element) {
			representation = base::strConcat("slice ", element.toString());
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType target, query::Context& ctx) const override;

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context&) const override;
	};

	class StaticArrayAbstractTypeImpl final: public AbstractTypeImpl {
		SymbolType<> element_type;
		usize        size;  // Number of elements in the array.

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::StaticArray;

		StaticArrayAbstractTypeImpl(const SymbolType<> element, const usize size):
			  element_type(element),
			  size(size) {
			representation = base::strConcat(element.toString(), "[", base::toString(size), "]");
		}

		[[nodiscard]]
		SymbolType<> getElementType() const {
			return element_type;
		}

		/**
		 * @brief Gets the compile-time constant size of the array.
		 * @return The size of the array.
		 */
		[[nodiscard]]
		usize getSize() const {
			return size;
		}

		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType target, query::Context&) const override;
		[[nodiscard]] bool isTriviallyDestructible(query::Context& ctx) const override;
		[[nodiscard]] bool isDefaultConstructible(query::Context& ctx) const override;
		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context& ctx) const override;
		[[nodiscard]] bool isCopyable(query::Context& ctx) const override;
		[[nodiscard]] bool isTriviallyCopyable(query::Context& ctx) const override;
		[[nodiscard]] bool carriesInformation(query::Context& ctx) const override;

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context& ctx) const override;
		[[nodiscard]] bool isDefaultConstructible(query::Context& ctx) const override;
		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context& ctx) const override;
		[[nodiscard]] bool isCopyable(query::Context& ctx) const override;
		[[nodiscard]] bool isTriviallyCopyable(query::Context& ctx) const override;

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override {
			// @TODO: #1273 this is a placeholder, implement proper logic
			return false;
		}

		/**
		 * @brief Function types are not default constructible, cause what even is a default function?
		 */
		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		/**
		 * @brief Function types are copyable, as they require a pointer/closure copy.
		 */
		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		/**
		 * @brief Function types are trivially copyable if they are not bound to any closure, this
		 * is just a pointer copy then. Otherwise it's not trivially copyable.
		 */
		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override {
			throw base::NotYetImplemented("isTriviallyCopyable for FunctionAbstractType");
		}

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	/** @TODO:
	 * Memory padding
	 * Dynamic "what am I?" information size based on input vector
	 * Sort variant types, so that var(A, B) = var(B, A)?
	 */
	class VariantAbstractTypeImpl final: public AbstractTypeImpl {
		std::vector<SymbolType<>>    underlying_types;
		base::Optional<SymbolType<>> optional_value_type;

	public:
		[[nodiscard]]
		Kind getKind() const override {
			return STATIC_KIND;
		}

		/**
		 * @brief The Kind of types described by objects of this class.
		 */
		static constexpr Kind STATIC_KIND = Kind::Variant;

		/**
		 * @brief Creates a variant of the given alternatives.
		 * @param represents_optional_type Marks the variant as the optional `?T`, in which case
		 * the first of @p variant_types has to be the held value type `T`.
		 */
		VariantAbstractTypeImpl(
			const std::vector<SymbolType<>>& variant_types, bool represents_optional_type
		);

		[[nodiscard]]
		const std::vector<SymbolType<>>& getUnderlyingTypes() const {
			return underlying_types;
		}

		[[nodiscard]]
		bool representsOptionalType() const {
			return optional_value_type.has_value();
		}

		[[nodiscard]]
		const SymbolType<>& getOptionalValueType() const {
			return optional_value_type.value();
		}

		[[nodiscard]]
		SymbolType<> getMember(const usize idx) const {
			return underlying_types[idx];
		}

		[[nodiscard]] bool isTriviallyDestructible(query::Context& ctx) const override;
		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override;
		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override;
		[[nodiscard]] bool isCopyable(query::Context& ctx) const override;
		[[nodiscard]] bool isTriviallyCopyable(query::Context& ctx) const override;

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;

		explicit ClassAbstractTypeImpl(compiler::helios::SymID symbol);

		[[nodiscard]]
		compiler::helios::SymID getSymbol() const {
			return symbol;
		}

		[[nodiscard]]
		compiler::helios::SymbolABI getABI(query::Context& ctx) const;

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

		[[nodiscard]] bool isTriviallyDestructible(query::Context& ctx) const override;
		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override;
		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override;
		[[nodiscard]] bool isCopyable(query::Context&) const override;
		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override;
		[[nodiscard]] bool carriesInformation(query::Context& ctx) const override;

	private:
		/**
		 * @brief The data of the class the type describes, holding its interface and the
		 * properties precomputed from it.
		 */
		[[nodiscard]]
		CRef<compiler::helios::ClassSymbolData> classSymbolData(query::Context& ctx) const;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return false; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override {
			// @TODO: #1275 this is a placeholder, implement proper logic
			return false;
		}

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return false; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		explicit MetaAbstractTypeImpl() { representation = "type"; }

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		/**
		 * @brief Meta type is default constructible with a `void` type.
		 */
		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return true; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
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

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return false; }

		[[nodiscard]]
		CRef<TypeInterface> getDeclaredInterface(query::Context& ctx) const override;
	};

	class TypeTemplateAbstractTypeImpl final: public AbstractTypeImpl {
		using Source = TypeTemplateAbstractType::Source;

		Source source;

	public:
		static constexpr Kind STATIC_KIND = Kind::TypeTemplate;

		[[nodiscard]] Kind getKind() const override { return STATIC_KIND; }

		TypeTemplateAbstractTypeImpl(Source source): source(source) {
			representation = base::strConcat("Type template: ", name(source).str());
		}

		[[nodiscard]] Source getSource() const { return source; }

		[[nodiscard]] bool carriesInformation(query::Context&) const override { return true; }

		[[nodiscard]] bool isTriviallyDestructible(query::Context&) const override { return true; }

		[[nodiscard]] bool isDefaultConstructible(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override {
			return false;
		}

		[[nodiscard]] bool isCopyable(query::Context&) const override { return false; }

		[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override { return false; }

		CRef<TypeInterface> getDeclaredInterface(query::Context&) const override;
	};
}
