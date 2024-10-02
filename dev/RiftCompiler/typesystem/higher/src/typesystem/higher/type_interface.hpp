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

#include <map>
#include <set>
#include <string>

#include <base/optional.hpp>

#include "type_info.hpp"
#include <helios/scope_symbol_id.hpp>
#include <base/string_id.hpp>
#include <variant>

namespace tsh {
	enum class Visibility { Public, Protected, Private };

	/**
	 * @brief A single element of an interface, defined by its symbol (not name).
	 */
	class InterfaceElement final {
	public:
		/**
		 * @brief A record which describes a parameter of a function.
		 *
		 * A parameter is described with its name, type, and whether it has a default value.
		 */
		struct Parameter final {
			base::StrID          name;
			TypeInfo             type;
			bool                 has_default_value;
			std::strong_ordering operator<=>(const Parameter& other) const = default;
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
		TypeInfo source;

		// @TODO: Add declaration order in source.
		// That is: Add information which describes the index of a field / method in
		// the declaration source code of a class / scope. The first declared field would
		// have index 0, the next one would have 1, etc.
		// This could be useful later on when addressing fields by index and when forming
		// packing strategies which the user can influence with the order of declarations.

		/**
		 * @brief The input parameters of this element of the interface.
		 * If the optional is empty, then the element is a field.
		 *
		 * Otherwise, if the vector inside the optional is empty, then it is a parameterless method.
		 */
		base::Optional<std::vector<Parameter>> parameters;

		/**
		 * @brief The type of a field or the return type of a method.
		 */
		TypeInfo result_type;

		/**
		 * @brief The visibility of an element of the interface.
		 *
		 * We want to keep information about all elements of the interface and their visibility,
		 * because we predict this will make error reporting easier.
		 * It's better to say "this element is present, but it is private and you cannot use it"
		 * rather than "this element is not recognised, go figure out why".
		 */
		Visibility visibility;

	public:
		/**
		 * @brief Construct an element of an interface of a type.
		 * @param parameters The parameters of this element.
		 * @param result_type The result type of this element.
		 * @param source The source of this element, i.e. the class which declares it.
		 * @param visibility The visibility level of this element.
		 */
		explicit InterfaceElement(
			const compiler::helios::SymID          symbol,
			const TypeInfo                         source,
			base::Optional<std::vector<Parameter>> parameters,
			const TypeInfo                         result_type,
			const Visibility                       visibility
		):
			  symbol(symbol),
			  source(source),
			  parameters(std::move(parameters)),
			  result_type(result_type),
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
		TypeInfo getSource() const {
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
			return parameters.empty();
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
			return parameters.has_value();
		}

		/**
		 * @brief Gets the parameter types of this element.
		 * @return The parameter types of this element.
		 */
		[[nodiscard]]
		base::Optional<const std::vector<Parameter>&> getParameters() const {
			return parameters.has_value()
			         ? base::Optional<const std::vector<Parameter>&>(parameters.value())
			         : base::Optional<const std::vector<Parameter>&>();
		}

		/**
		 * @brief Gets the result type of this element.
		 * @return The result type of this element.
		 */
		[[nodiscard]]
		TypeInfo getResultType() const {
			return result_type;
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
		TypeInfo getType(query::Context& ctx) const;

		/**
		 * @brief Gets the visibility of this element.
		 * @return The visibility of this element.
		 */
		[[nodiscard]]
		Visibility getVisibility() const {
			return visibility;
		}

		/**
		 * String ordering of InterfaceElement.
		 * @param other The other InterfaceElement.
		 * @return A strong_ordering result.
		 *
		 * @note We can guarantee the ordering to be strong because all the components can be
		 * strongly ordered. Including Optionals, vectors, and Parameters.
		 */
		std::strong_ordering operator<=>(const InterfaceElement& other) const = default;
	};

	/**
	 * @brief An aggregate of the elements of the interface of an object.
	 *
	 * @note Expected to be used predominantly for symbol resolution in type-dependent contexts.
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
		const base::Map<base::StrID, std::set<InterfaceElement>> elements{};

	public:
		TypeInterface() = default;

		/**
		 * @brief Construct the interface of a type from the elements of that interface.
		 * @param elements The elements of the interface.
		 */
		explicit TypeInterface(const std::set<InterfaceElement>& elements);

		/**
		 * @brief Gets all the elements of an interface, grouped by name.
		 * @return The elements of an interface, grouped by name.
		 */
		[[nodiscard]]
		const base::Map<base::StrID, std::set<InterfaceElement>>& getElements() const {
			return elements;
		}

		/**
		 * @brief Gets all the elements of an interface with a given name.
		 * @param name The requested name.
		 * @return The elements of an interface with the requested name.
		 */
		[[nodiscard]]
		const std::set<InterfaceElement>& getElements(base::StrID name) {
			static std::set<InterfaceElement> empty_set{};
			if (!elements.contains(name)) return empty_set;
			return elements.at(name);
		}

		/*-------------------------*\
		|    OVERLOAD RESOLUTION    |
		\*-------------------------*/

		/**
		 * @brief A record which describes a named argument provided to a function call.
		 *
		 * A named argument is described with its name and type.
		 */
		struct NamedArgument final {
			base::StrID name;
			TypeInfo    type;
		};

		/**
		 * @brief A resolution result which means that no match was found.
		 */
		struct NoMatch final {
			/**
			 * @brief Elements with the requested name.
			 */
			std::set<InterfaceElement> non_matches;
		};

		/**
		 * @brief A resolution result which means that a single match was found.
		 */
		struct SingleMatch final {
			/**
			 * @brief The best match.
			 */
			InterfaceElement best_match;

			/**
			 * @brief Other members of the same name which matched less accurately.
			 */
			std::set<InterfaceElement> alternative_matches;

			/**
			 * @brief Elements with the requested name which did not match.
			 */
			std::set<InterfaceElement> non_matches;
		};

		/**
		 * @brief A resolution result which means that member resolution is ambiguous.
		 *
		 * @note An exact match is still possible when the resolution is ambiguous. Consider
		 * the overloaded function `foo` with signatures `foo(a : i32, b : bool = true)` and
		 * `foo(a : i32, c : char = 'a')`. A call of `foo(2)` matches both signatures perfectly,
		 * but remains ambiguous.
		 */
		struct AmbiguousMatch final {
			/**
			 * @brief The conflicting matches.
			 */
			std::set<InterfaceElement> conflicting_matches;

			/**
			 * @brief Other members of the same name which matched less accurately.
			 */
			std::set<InterfaceElement> alternative_matches;

			/**
			 * @brief Elements with the requested name which did not match.
			 */
			std::set<InterfaceElement> non_matches;
		};

		using ResolutionResult = std::variant<NoMatch, SingleMatch, AmbiguousMatch>;

		/**
		 * @brief Gets the elements which match a name.
		 *
		 * @note This should be the primary method of resolving fields (as opposed to methods).
		 *
		 * @param name The requested name of an element.
		 * @param ctx The query context for implicit coercion checks.
		 * @return The elements which match the name.
		 */
		ResolutionResult resolve(base::StrID name, query::Context& ctx);

		/**
		 * @brief Gets the elements which match a name and given arguments.
		 *
		 * @note This should not be used to resolve fields. This function assumes that it is
		 * resolving a method, with perhaps an empty parameter list.
		 *
		 * @param name The requested name of an element.
		 * @param positional_arg_types The types of the supplied positional arguments.
		 * @param named_args The supplied named arguments.
		 * @param ctx The query context for implicit coercion checks.
		 * @return The elements which match the name.
		 */
		ResolutionResult resolve(
			base::StrID                    name,
			const std::vector<TypeInfo>&   positional_arg_types,
			const std::set<NamedArgument>& named_args,
			query::Context&                ctx
		);

		/**
		 * @brief Gets the elements which match a name and a single given argument.
		 *
		 * Only methods which can take exactly one argument are considered. In particular, methods
		 * which *might* take one argument (for example, all other arguments are defaulted), are
		 * not considered and do not affect resolution ambiguity.
		 *
		 * @param name The requested name of an element.
		 * @param single_arg_type The type of a single argument.
		 * @param ctx The query context for implicit coercion checks.
		 * @return The elements which match the name.
		 */
		ResolutionResult resolve(base::StrID name, TypeInfo single_arg_type, query::Context& ctx);

		/**
		 * @brief Auxiliary function to stringify a member lookup request.
		 * @param name The name of the requested member.
		 * @param argument_info The arguments provided.
		 * Empty optional if no arguments list was provided.
		 * Optional with empty argument lists if an empty argument list was provided.
		 * The first list represents the types of the positional arguments,
		 * while the second list represents the named arguments.
		 * @return The stringified signature.
		 *
		 * For example:
		 *
		 * The request `obj.mem` for member `mem` in object `obj` corresponds to
		 * `name = mem` and `argument_info = {}`, and stringifies to `"mem"`.
		 *
		 * The request `obj.foo()` corresponds to
		 * `name = foo` and `argument_info = {{},{}}`, and stringifies to `foo()`.
		 *
		 * The request `obj.foo(1, 3.14, print_result = true)` corresponds to
		 * `name = foo` and `argument_info = {{i32, f32}, {{"print_result", bool}}}`
		 * (notation simplified), and stringifies to `foo(i32, f32, print_result : bool)`.
		 */
		static std::string stringifyRequestSignature(
			base::StrID name,
			const base::Optional<
				std::pair<std::vector<TypeInfo>, std::vector<TypeInterface::NamedArgument>>>&
				argument_info
		);
	};
}
