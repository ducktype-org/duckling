#pragma once

#include <map>
#include <set>
#include <string>

#include <base/optional.hpp>

#include "type_info.hpp"

namespace ts {
	enum class Visibility { Public, Protected, Private };

	class InterfaceElement final {
		/**
		 * \brief The input parameter types of this element of the interface.
		 * If the optional is empty, then the element is a field.
		 *
		 * Otherwise, if the vector inside the optional is empty, then it is a parameterless method.
		 */
		const base::Optional<std::vector<TypeInfo>> parameter_types;

		/**
		 * \brief The type of a field or the return type of a method.
		 */
		const TypeInfo result_type;

		/**
		 * \brief Where the element was declared.
		 *
		 * For example, if a class A implements an interface I which defines method foo,
		 * then it is important that the foo method in A is in actuality I.foo.
		 * In this context, I is the source of A.foo.
		 */
		const TypeInfo source;

		/**
		 * \brief The visibility of an element of the interface.
		 *
		 * We want to keep information about all elements of the interface and their visibility,
		 * because we predict this will make error reporting easier.
		 * It's better to say "this element is present, but it is private and you cannot use it"
		 * rather than "this element is not recognised, go figure out why".
		 */
		const Visibility visibility;

		/**
		 * \brief Construct an element of an interface of a type.
		 * \param parameter_types The parameter types of this element.
		 * \param result_type The result type of this element.
		 * \param source The source of this element, i.e. the class which declares it.
		 * \param visibility The visibility level of this element.
		 */
		explicit InterfaceElement(
			base::Optional<std::vector<TypeInfo>> parameter_types,
			const TypeInfo                        result_type,
			const TypeInfo                        source,
			const Visibility                      visibility
		):
			  parameter_types(std::move(parameter_types)),
			  result_type(result_type),
			  source(source),
			  visibility(visibility) {}

		/**
		 * \brief Checks if this element of the interface is a field.
		 *
		 * An element is a field if it cannot be called (unlike a method).
		 * Equivalently, its value is stored in memory instead of being computed every time.
		 *
		 * This is always equal to `!isMethod()`.
		 * \return Whether this element of the interface is a field.
		 */
		[[nodiscard]]
		bool isField() const {
			return parameter_types.empty();
		}

		/**
		 * \brief Checks if this element of the interface is a method.
		 *
		 * An element is a method if it must be called to get its value (unlike a field).
		 * Equivalently, its value is computed anew every time instead of being stored in memory.
		 *
		 * This is always equal to `!isField()`.
		 * \return Whether this element of the interface is a method.
		 */
		[[nodiscard]]
		bool isMethod() const {
			return parameter_types.has_value();
		}

		/**
		 * \brief Gets the parameter types of this element.
		 * \return The parameter types of this element.
		 */
		[[nodiscard]]
		base::Optional<const std::vector<TypeInfo>&> getParameterTypes() const {
			return parameter_types.has_value()
			         ? base::Optional<const std::vector<TypeInfo>&>(parameter_types.value())
			         : base::Optional<const std::vector<TypeInfo>&>();
		}

		/**
		 * \brief Gets the result type of this element.
		 * \return The result type of this element.
		 */
		[[nodiscard]]
		TypeInfo getResultType() const {
			return result_type;
		}

		/**
		 * \brief Gets the source of this element.
		 * \return The source of this element.
		 */
		[[nodiscard]]
		TypeInfo getSource() const {
			return source;
		}

		/**
		 * \brief Gets the visibility of this element.
		 * \return The visibility of this element.
		 */
		[[nodiscard]]
		Visibility getVisibility() const {
			return visibility;
		}
	};

	class TypeInterface final {
		/**
		 * \brief The collection of elements of the interface of a type.
		 */
		const std::map<std::string, std::set<InterfaceElement>> elements{};

	public:
		TypeInterface() = default;

		/**
		 * \brief Construct the interface of a type from the elements of that interface.
		 * \param elements The elements of the interface.
		 */
		explicit TypeInterface(std::map<std::string, std::set<InterfaceElement>> elements):
			  elements(std::move(elements)) {}
	};
}
