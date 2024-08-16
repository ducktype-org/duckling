#include "type_interface.hpp"

#include "queries/types.hpp"

#include <helios/symbols/symbols.hpp>
#include <query_framework/query_impl.hpp>

namespace ts {
	namespace {
		base::Map<base::StrId, std::set<InterfaceElement>>
			groupElementsByName(const std::set<InterfaceElement>& elements) {
			base::Map<base::StrId, std::set<InterfaceElement>> result{};
			for (const InterfaceElement& element: elements) {
				base::StrId name = compiler::helios::name(element.getSymbol());
				if (!result.contains(name)) result.put(name, {});
				result.at(name).insert(element);
			}
			return result;
		}
	}

	TypeInterface::TypeInterface(const std::set<InterfaceElement>& elements):
		  elements(groupElementsByName(elements)) {}

	TypeInfo InterfaceElement::getType(query::Context& ctx) const {
		if (isField())
			return getResultType();
		else {
			// @TODO: Add .is_mutable and .pure when additional method specifiers are supported.
			std::vector<TypeInfo> all_parameter_types{};
			all_parameter_types.push_back(ctx.query<QueryPointerType>({
				.type       = source,
				.is_mutable = false,
			}));
			for (const auto& par: parameters.value()) all_parameter_types.push_back(par.type);
			return ctx.query<QueryFunctionType>({
				.parameter_types = all_parameter_types,
				.result_type     = result_type,
				.pure            = false,
				.free            = false,
			});
		}
	}

	using ResolutionResult = TypeInterface::ResolutionResult;
	using NamedArgument    = TypeInterface::NamedArgument;
	using Parameter        = InterfaceElement::Parameter;

	ResolutionResult TypeInterface::resolve(base::StrId name, query::Context&) {
		const std::set<InterfaceElement>& elements_matching_name = getElements(name);
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
	std::set<InterfaceElement>& selectMatchCategoryForMethod(
		const std::vector<TypeInfo>&   positional_arg_types,
		const std::set<NamedArgument>& named_args,
		query::Context&                ctx,
		const InterfaceElement&        method,
		std::set<InterfaceElement>&    exact_matches,
		std::set<InterfaceElement>&    coercion_matches,
		std::set<InterfaceElement>&    non_matches
	) {
		// Since this resolution step really only considers methods, we discard fields.
		if (method.isField()) return non_matches;

		std::vector<Parameter> parameters = method.getParameters().value();
		std::vector<bool>      param_was_provided(parameters.size());
		bool                   coercion_present = false;

		// Too many parameters case.
		if (positional_arg_types.size() + named_args.size() > parameters.size()) return non_matches;

		// Go over positional arguments.
		for (int i = 0; i < positional_arg_types.size(); i++) {
			TypeInfo provided_type = positional_arg_types[i];
			TypeInfo expected_type = parameters[i].type;
			if (provided_type != expected_type) {
				// Type mismatch case.
				if (!ctx.query<QueryImplicitCoercibilityOnInfo>({ provided_type, expected_type }))
					return non_matches;
				coercion_present = true;
			}

			param_was_provided[i] = true;
		}

		// Go over named arguments.
		for (auto named_arg: named_args) {
			int param_with_matching_name_idx = -1;
			for (int i = 0; i < parameters.size(); i++) {
				auto param = parameters[i];
				if (!param_was_provided[i] && param.name == named_arg.name) {
					param_with_matching_name_idx = i;
					break;
				}
			}

			// Name mismatch case.
			if (param_with_matching_name_idx == -1) return non_matches;
			TypeInfo provided_type = named_arg.type;
			TypeInfo expected_type = parameters[param_with_matching_name_idx].type;
			if (provided_type != expected_type) {
				// Type mismatch case.
				if (!ctx.query<QueryImplicitCoercibilityOnInfo>({ provided_type, expected_type }))
					return non_matches;
				coercion_present = true;
			}
			param_was_provided[param_with_matching_name_idx] = true;
		}

		// Check that all unprovided parameters have default values.
		for (int i = 0; i < parameters.size(); i++)
			if (!param_was_provided[i] && !parameters[i].has_default_value) return non_matches;

		return coercion_present ? coercion_matches : exact_matches;
	}

	ResolutionResult TypeInterface::resolve(
		base::StrId                    name,
		const std::vector<TypeInfo>&   positional_arg_types,
		const std::set<NamedArgument>& named_args,
		query::Context&                ctx
	) {
		// Preamble
		const std::set<InterfaceElement>& elements_matching_name = getElements(name);

		std::set<InterfaceElement> exact_matches;
		std::set<InterfaceElement> coercion_matches;
		std::set<InterfaceElement> non_matches;

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
				.insert(element);
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

	ResolutionResult
		TypeInterface::resolve(base::StrId name, TypeInfo single_arg_type, query::Context& ctx) {
		return resolve(name, { single_arg_type }, {}, ctx);
	}

	std::string TypeInterface::stringifyRequestSignature(
		base::StrId name,
		const base::Optional<
			std::pair<std::vector<TypeInfo>, std::vector<TypeInterface::NamedArgument>>>&
			argument_info
	) {
		std::stringstream result;
		result << name.str();

		if (argument_info.has_value()) {
			const std::vector<TypeInfo>& positional                = argument_info.value().first;
			const std::vector<TypeInterface::NamedArgument>& named = argument_info.value().second;

			std::vector<std::string> args;
			args.reserve(positional.size() + named.size());
			for (auto positional_type: positional) args.push_back(positional_type.toString());
			for (auto named_arg: named)
				args.push_back(named_arg.name.str() + " : " + named_arg.type.toString());

			result << "(";
			for (int i = 0; i < args.size() - 1; i++) result << args[i] << ", ";
			if (!args.empty()) result << args[args.size() - 1];
			result << ")";
		}

		return result.str();
	}
}
