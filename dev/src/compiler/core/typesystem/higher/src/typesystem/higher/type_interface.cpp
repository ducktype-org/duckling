#include "type_interface.hpp"

#include "queries.hpp"

#include <helios/symbols/simple.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context.hpp>

namespace tsh {
	namespace {
		base::Map<base::StrID, std::vector<InterfaceElement>> groupElementsByName(
			const std::vector<InterfaceElement>& elements
		) {
			base::Map<base::StrID, std::vector<InterfaceElement>> result{};
			for (const InterfaceElement& element: elements) {
				base::StrID name = compiler::helios::name(element.getSymbol());
				if (!result.contains(name)) result.put(name, {});
				result.at(name).push_back(element);
			}
			return result;
		}
	}

	TypeInterface::TypeInterface(const std::vector<InterfaceElement>& elements):
		  elements(elements),
		  elements_by_name(groupElementsByName(elements)) {}

	const base::Map<base::StrID, std::vector<InterfaceElement>>& TypeInterface::getElementsByName(
	) const {
		return elements_by_name;
	}

	const std::vector<InterfaceElement>& TypeInterface::getElementsWithName(const base::StrID name
	) const {
		static constexpr std::vector<InterfaceElement> EMPTY{};
		if (elements_by_name.contains(name)) return elements_by_name.at(name);
		return EMPTY;
	}

	SymbolType<> InterfaceElement::getType(query::Context& ctx) const {
		if (isField()) return getResultType();

		// @TODO: #1396 Add .is_mutable and .pure when additional method specifiers are supported.
		std::vector<SymbolType<>> all_parameter_types{};
		// @note: The first parameter is the implicit self parameter. It might change to
		// being specified in the method declaration.
		all_parameter_types.emplace_back(source, ReferenceKind::Ref, Mutability::Mutable);
		for (const auto& par: parameters.value()) all_parameter_types.push_back(par.type);
		return SymbolType{
			ctx.query<QueryFunctionType>({
				.parameter_types = all_parameter_types,
				.result_type     = result_type,
				.pure            = false,
				.free            = false,
			}),
			ReferenceKind::Direct,
			Mutability::Immutable,
		};
	}

	using ResolutionResult = TypeInterface::ResolutionResult;
	using NamedArgument    = TypeInterface::NamedArgument;
	using Parameter        = InterfaceElement::Parameter;

	ResolutionResult TypeInterface::resolve(const base::StrID name, query::Context&) const {
		const std::vector<InterfaceElement>& elements_matching_name = getElementsWithName(name);
		if (elements_matching_name.empty()) return NoMatch{ {} };
		if (elements_matching_name.size() == 1)
			return SingleMatch{ *elements_matching_name.begin(), {}, {} };
		return AmbiguousMatch{ elements_matching_name, {}, {} };
	}

	/**
	 * @brief For an InterfaceElement which is a method (i.e. which has parameters), given two
	 * collections of positional and named arguments, determine whether the arguments match the
	 * parameters exactly, via coercion, or not at all. Return the proper category by reference.
	 * @param positional_arg_types The types of the provided positional arguments.
	 * @param named_args The names and types of the provided named arguments.
	 * @param ctx The query context, for coercibility resolution.
	 * @param method The method to be matched against.
	 * @param exact_matches The reference to the category of exact matches.
	 * @param coercion_matches The reference to the category of matches via coercion.
	 * @param non_matches The reference to the category of non-matches.
	 * @return The reference to the proper category, out of the three given.
	 */
	std::vector<InterfaceElement>& selectMatchCategoryForMethod(
		const std::vector<AbstractType>&  positional_arg_types,
		const std::vector<NamedArgument>& named_args,
		query::Context&                   ctx,
		const InterfaceElement&           method,
		std::vector<InterfaceElement>&    exact_matches,
		std::vector<InterfaceElement>&    coercion_matches,
		std::vector<InterfaceElement>&    non_matches
	) {
		// Since this resolution step really only considers methods, we discard fields.
		if (method.isField()) return non_matches;

		const std::vector<Parameter> parameters = *method.getParameters().value();
		std::vector<bool>            param_was_provided(parameters.size());
		bool                         coercion_present = false;

		// Too many parameters case.
		if (positional_arg_types.size() + named_args.size() > parameters.size()) return non_matches;

		// Go over positional arguments.
		for (usize i = 0; i < positional_arg_types.size(); i++) {
			AbstractType provided_type = positional_arg_types[i];
			if (AbstractType expected_type = parameters[i].type.getType();
			    provided_type != expected_type) {
				// Type mismatch case.
				if (!ctx.query<QueryImplicitCoercibilityOnAbstractType>({ provided_type,
				                                                          expected_type }))
					return non_matches;
				coercion_present = true;
			}

			param_was_provided[i] = true;
		}

		// Go over named arguments.
		for (auto [name, type]: named_args) {
			base::Optional<u32> param_with_matching_name_idx{};
			for (u32 i = 0; i < parameters.size(); i++) {
				if (auto param = parameters[i]; !param_was_provided[i] && param.name == name) {
					param_with_matching_name_idx = i;
					break;
				}
			}

			// Name mismatch case.
			if (param_with_matching_name_idx.empty()) return non_matches;
			AbstractType provided_type = type;
			AbstractType expected_type
				= parameters[param_with_matching_name_idx.value()].type.getType();
			if (provided_type != expected_type) {
				// Type mismatch case.
				if (!ctx.query<QueryImplicitCoercibilityOnAbstractType>({ provided_type,
				                                                          expected_type }))
					return non_matches;
				coercion_present = true;
			}
			param_was_provided[param_with_matching_name_idx.value()] = true;
		}

		// Check that all unprovided parameters have default values.
		for (u32 i = 0; i < parameters.size(); i++)
			if (!param_was_provided[i] && !parameters[i].has_default_value) return non_matches;

		return coercion_present ? coercion_matches : exact_matches;
	}

	ResolutionResult TypeInterface::resolve(
		const base::StrID                 name,
		const std::vector<AbstractType>&  positional_arg_types,
		const std::vector<NamedArgument>& named_args,
		query::Context&                   ctx
	) const {
		// Preamble
		const std::vector<InterfaceElement>& elements_matching_name = getElementsWithName(name);

		std::vector<InterfaceElement> exact_matches;
		std::vector<InterfaceElement> coercion_matches;
		std::vector<InterfaceElement> non_matches;

		// Categorise overloads into the above sets.
		for (const auto& element: elements_matching_name) {
			selectMatchCategoryForMethod(
				positional_arg_types,
				named_args,
				ctx,
				element,
				exact_matches,
				coercion_matches,
				non_matches
			)
				.push_back(element);
		}

		// Consider no match scenarios.
		if (exact_matches.empty() && coercion_matches.empty()) return NoMatch{ non_matches };

		// Consider single match scenarios.
		if (exact_matches.size() == 1)
			return SingleMatch{ *exact_matches.begin(), coercion_matches, non_matches };
		if (exact_matches.empty() && coercion_matches.size() == 1)
			return SingleMatch{ *coercion_matches.begin(), {}, non_matches };

		// Consider ambiguous match scenarios.
		if (exact_matches.empty()) return AmbiguousMatch{ coercion_matches, {}, non_matches };
		return AmbiguousMatch{ exact_matches, coercion_matches, non_matches };
	}

	ResolutionResult TypeInterface::resolve(
		const base::StrID name, AbstractType single_arg_type, query::Context& ctx
	) const {
		return resolve(name, { single_arg_type }, {}, ctx);
	}

	std::string TypeInterface::stringifyRequestSignature(
		const base::StrID request_name,
		const base::Optional<std::pair<std::vector<AbstractType>, std::vector<NamedArgument>>>&
			argument_info
	) {
		std::stringstream result;
		result << request_name.str();

		if (argument_info.has_value()) {
			const std::vector<AbstractType>&  positional = argument_info.value().first;
			const std::vector<NamedArgument>& named      = argument_info.value().second;

			std::vector<std::string> args;
			args.reserve(positional.size() + named.size());
			for (auto positional_type: positional) args.push_back(positional_type.toString());
			for (auto [arg_name, type]: named)
				args.push_back(arg_name.str() + " : " + type.toString());

			result << "(";
			for (u32 i = 0; i < args.size() - 1; i++) result << args[i] << ", ";
			if (!args.empty()) result << args[args.size() - 1];
			result << ")";
		}

		return result.str();
	}
}
