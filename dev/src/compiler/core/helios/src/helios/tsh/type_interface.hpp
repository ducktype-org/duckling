/**
 * @file type_interface.hpp
 * @brief The interface provided by a type consists of
 * its attributes and methods. In type system implementation,
 * type interfaces are represented by the TypeInterface class.
 *
 * In practice, TypeInterface simply holds a map,
 * which associates InterfaceElement elements with their names.
 */
#pragma once

#include "abstract_type.hpp"
#include "symbol_type.hpp"

#include <helios/symbols/symbol_id.hpp>

#include <string_id/string_id.hpp>

#include <ranges>

namespace compiler::tsh {
	enum class ClassMemberVisibility { Public, Protected, Private };

	/**
	 * @brief A single element of a type interface, defined by its symbol (not name).
	 */
	class InterfaceElement final {
	public:
		/**
		 * The kind of a type interface element.
		 * @note In the future we might add more class-specific kinds here,
		 * such as for example special-methods, base classes, etc.
		 */
		enum class InterfaceElementKind {
			Field,
			Method,

			/**
			 * Other elements are symbols that are not "part of" a type
			 * in a direct way, but are declared within the class body.
			 * For example: constants, using declarations
			 */
			Other,
		};

	private:
		/**
		 * @brief The symbol corresponding to this element.
		 */
		compiler::helios::SymID symbol;

		/**
		 * @brief Where the element was declared.
		 *
		 * For example, if a class A implements an interface I which defines method foo,
		 * then it is important that the foo method in A is in actuality I.foo.
		 * In this context, I is the source of A.foo.
		 */
		AbstractType source;

		/**
		 * The index of a field / method in the declaration source code of a class / scope.
		 * Is relevant mostly for fields.
		 */
		u32 declaration_order;

		InterfaceElementKind kind;

		/**
		 * @brief The visibility of an element of the interface.
		 *
		 * We want to keep information about all elements of the interface and their visibility,
		 * because we predict this will make error reporting easier.
		 * It's better to say "this element is present, but it is private and you cannot use it"
		 * rather than "this element is not recognised, go figure out why".
		 */
		ClassMemberVisibility visibility;

	public:
		/**
		 * @brief Construct an element of an interface of a type.
		 * @param symbol The symbol of this element.
		 * @param parameters The parameters of this element.
		 * @param declaration_order The index of this element in the declaration source code.
		 * @param result_type The result type of this element.
		 * @param source The source of this element, i.e. the class which declares it.
		 * @param visibility The visibility level of this element.
		 */
		explicit InterfaceElement(
			const compiler::helios::SymID symbol,
			const AbstractType            source,
			const u32                     declaration_order,
			const InterfaceElementKind    kind,
			const ClassMemberVisibility   visibility
		):
			  symbol(symbol),
			  source(source),
			  declaration_order(declaration_order),
			  kind(kind),
			  visibility(visibility) {}

		/**
		 * @brief Gets the symbol of this element.
		 * @return The symbol of this element.
		 */
		[[nodiscard]]
		compiler::helios::SymID getSymbol() const {
			return symbol;
		}

		/**
		 * @brief Gets the source of this element.
		 * @return The source of this element.
		 */
		[[nodiscard]]
		AbstractType getSource() const {
			return source;
		}

		/**
		 * @brief Checks if this element of the interface is a field.
		 *
		 * An element is a field if it cannot be called (unlike a method).
		 * Equivalently, its value is stored in memory instead of being computed every time.
		 *
		 * This is always equal to `!isMethod()`.
		 * @return Whether this element of the interface is a field.
		 */
		[[nodiscard]]
		bool isField() const {
			return kind == InterfaceElementKind::Field;
		}

		/**
		 * @brief Checks if this element of the interface is a method.
		 *
		 * An element is a method if it must be called to get its value (unlike a field).
		 * Equivalently, its value is computed anew every time instead of being stored in memory.
		 *
		 * This is always equal to `!isField()`.
		 * @return Whether this element of the interface is a method.
		 */
		[[nodiscard]]
		bool isMethod() const {
			return kind == InterfaceElementKind::Method;
		}

		/**
		 * @brief Gets the entire type of this element.
		 *
		 * For example, if this element is a field, then its type is simply the return type.
		 * However, if it's a method, then the type is a Function object type with an implicit
		 * argument of the type of the host class.
		 *
		 * @param ctx The query Context required to create function types.
		 * @return The type of this element.
		 */
		[[nodiscard]]
		SymbolType<> getType(query::Context& ctx) const;

		/**
		 * @brief Gets the visibility of this element.
		 * @return The visibility of this element.
		 */
		[[nodiscard]]
		ClassMemberVisibility getVisibility() const {
			return visibility;
		}

		/**
		 * String ordering of InterfaceElement.
		 * @param other The other InterfaceElement.
		 * @return A strong_ordering result.
		 *
		 * @note We can guarantee the ordering to be strong because all the fields can be
		 * strongly ordered. Including Optionals, vectors, and Parameters.
		 */
		std::strong_ordering operator<=>(const InterfaceElement& other) const = default;
	};

	/**
	 * @brief An aggregate of the elements of the interface of an object.
	 *
	 * @note Expected to be used predominantly for getting structural information about the type
	 * content.
	 * @important It is used in type-instance lookup,
	 * but it does not implement the lookup logic directly, and should not be used for that.
	 * Use HInterface for that instead.
	 *
	 * Full information about all elements of an interface is obtained via the `getElements` method.
	 * The returned map is indexed by string IDs (instead of symbols) because element names may be
	 * overloaded and resolved only in a typing context (e.g. call of an overloaded method with
	 * arguments of known types).
	 */
	class TypeInterface final {
		/**
		 * @brief The collection of elements of the interface of a type.
		 */
		std::vector<InterfaceElement> elements;

		/**
		 * @brief The collection of elements of the interface, grouped by name.
		 * @note This is duplicated from `elements` for performance reasons.
		 * \parallel Accessed when building and querying a \ref TypeInterface; should be safe if
		 * \ref TypeInterface instances are shared across threads.
		 */
		base::Map<base::StrID, std::vector<InterfaceElement>> elements_by_name;

		/**
		 * Check if the element list of the interface contains duplicates.
		 * Note: it shouldn't. Use this for assertions in constructors.
		 */
		[[nodiscard]]
		base::OkBad checkForDuplicates() const;

	public:
		TypeInterface() = default;

		/**
		 * @brief Construct the interface of a type from the elements of that interface.
		 * @param elements The elements of the interface.
		 */
		explicit TypeInterface(const std::vector<InterfaceElement>& elements);

		/**
		 * Create a new interface which contains all elements from this one, as well as all elements
		 * from another interface, in this order (note: order of elements is important in interfaces).
		 * @param other The other type interface.
		 * @return The new type interface, containing all elements from this one, and the `other`.
		 */
		[[nodiscard]]
		TypeInterface combine(CRef<TypeInterface> other) const;

		[[nodiscard]]
		const std::vector<InterfaceElement>& getElements() const {
			return elements;
		}

		/**
		 * @brief Gets all the elements of an interface, grouped by name.
		 * @return The elements of an interface, grouped by name.
		 */
		[[nodiscard]]
		const base::Map<base::StrID, std::vector<InterfaceElement>>& getElementsByName() const;

		/**
		 * @brief Gets all the elements of an interface with a given name.
		 * @param name The requested name.
		 * @return The elements of an interface with the requested name.
		 */
		[[nodiscard]]
		const std::vector<InterfaceElement>& getElementsWithName(base::StrID name) const;

		/**
		 * @brief Gets a view of all the fields of this interface.
		 * @return A view of all the fields of this interface.
		 */
		[[nodiscard]]
		auto getFieldsView() const {
			return elements
			     | std::views::filter([](const InterfaceElement& e) { return e.isField(); });
		}

		/**
		 * @brief Gets a view of all the methods of this interface.
		 * @return A view of all the methods of this interface.
		 */
		[[nodiscard]]
		auto getMethodsView() const {
			return elements
			     | std::views::filter([](const InterfaceElement& e) { return e.isMethod(); });
		}
	};
}
