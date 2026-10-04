#pragma once

#include "diagnostic_arguments.hpp"

#include <concepts>
#include <iostream>

namespace dia::dia_args {
	template<typename TargetComponent>
	requires std::derived_from<TargetComponent, Component> void forEachComponentRecursive(
		Component& component, std::invocable<TargetComponent&> auto&& func
	) {
		if (auto* target = dynamic_cast<TargetComponent*>(&component)) func(*target);

		if (auto* concat = dynamic_cast<ConcatComponent*>(&component)) {
			for (auto& element: concat->elements)
				forEachComponentRecursive<TargetComponent>(*element, func);
			return;
		}

		if (auto* pointed = dynamic_cast<PointedComponent*>(&component)) {
			forEachComponentRecursive<TargetComponent>(*pointed->content, func);
			return;
		}

		if (auto* variant = dynamic_cast<VariantComponent*>(&component)) {
			forEachComponentRecursive<TargetComponent>(*variant->content, func);
			forEachComponentRecursive<TargetComponent>(*variant->alt_content, func);
			return;
		}

		if (auto* link = dynamic_cast<LinkComponent*>(&component)) {
			forEachComponentRecursive<TargetComponent>(*link->content, func);
			return;
		}
	}

	template<typename TargetComponent>
	requires std::derived_from<TargetComponent, Component>
	void forEachComponentInMessage(Message& message, std::invocable<TargetComponent&> auto&& func) {
		for (auto& [_, argument]: message.arguments)
			forEachComponentRecursive<TargetComponent>(*argument, func);

		for (auto& explore_link: message.explore_links)
			for (auto& [__, param]: explore_link.params)
				forEachComponentRecursive<TargetComponent>(*param, func);
	}

	template<typename TargetComponent>
	requires std::derived_from<TargetComponent, Component> void forEachComponentInDiagnostic(
		Diagnostic& diagnostic, std::invocable<TargetComponent&> auto&& func
	) {
		forEachComponentInMessage<TargetComponent>(diagnostic.main_message, func);

		for (auto& [_, message]: diagnostic.linked_messages)
			forEachComponentInMessage<TargetComponent>(message, func);
	}
}  // namespace dia::dia_args
