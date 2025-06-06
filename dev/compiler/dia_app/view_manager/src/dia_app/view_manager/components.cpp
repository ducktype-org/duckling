#include "components.hpp"

#include "side_panel.hpp"
#include "utils.hpp"

#include <proto/view.pb.h>

namespace dia_app {
	namespace view_manager {
		
		component_id_t getNewId() {
			static component_id_t next = 0;
			return next++;
		}

		// Component
		Component::Component(
			std::weak_ptr<ViewConstructor> view_constructor
		):
			  view_constructor(std::move(view_constructor)) {}

		Component::~Component() {}

		std::shared_ptr<Component> Component::deepCopy() {
			assert(false);
			return {};
		}

		component_get_view_data_t<::view::HlComponent> Component::getHlView() const {
			assert(false);
			return {};
		}

		component_get_view_data_t<::view::NoHlComponent> Component::getNoHlView() const {
			assert(false);
			return {};
		}

		void Component::registerInteraction(
			InteractionType interaction_type, InteractionContext& interaction_context
		) {
			auto strong_parent = parent.lock();
			if (strong_parent)
				strong_parent->registerInteraction(interaction_type, interaction_context);
		}

		// TextComponent

		TextComponent::TextComponent(
			std::weak_ptr<ViewConstructor> view_constructor,
			std::string                    content,
			std::vector<side_entry_id_t>   assoc_side_entries
		):
			  Component(std::move(view_constructor)),
			  content(std::move(content)),
			  assoc_side_entries(std::move(assoc_side_entries)) {}

		component_get_view_data_t<::view::NoHlComponent> TextComponent::getNoHlView() const {
			debug("TextComponent::getNoHlView() begin");
			auto result = std::make_unique<::view::NoHlComponent>();
			result->mutable_text_component()->set_content(this->content);
			result->mutable_text_component()->mutable_assoc_side_entries()->Add(
				this->assoc_side_entries.begin(), this->assoc_side_entries.end()
			);
			auto left = line_suffix_data_t<::view::NoHlComponent>{ std::move(result) };
			auto mid  = std::vector<line_data_t<::view::NoHlComponent>>{};
			debug("TextComponent::getNoHlView() end");
			return { std::move(left), std::move(mid) };
		}

		std::shared_ptr<Component> TextComponent::deepCopy() {
			auto result = std::make_shared<TextComponent>(this->view_constructor, this->content, this->assoc_side_entries);
			result->parent = this->parent;
			return std::static_pointer_cast<Component>(result);
		}

		void TextComponent::registerInteraction(
			InteractionType interaction_type, InteractionContext& interaction_context
		) {
			if (interaction_type == InteractionType::Click)
				openSideEntries(this->assoc_side_entries, this->view_constructor, interaction_context);
			else
				Component::registerInteraction(interaction_type, interaction_context);
		}

		// CodeComponent

		CodeComponent::CodeComponent(
            std::weak_ptr<ViewConstructor> view_constructor,
			std::string                  content,
			std::vector<hl_id_t>         tags,
			std::vector<side_entry_id_t> assoc_side_entries
		):
			  Component(std::move(view_constructor)),
			  content(std::move(content)),
			  tags(std::move(tags)),
			  assoc_side_entries(std::move(assoc_side_entries)) {}

		component_get_view_data_t<::view::HlComponent> CodeComponent::getHlView() const {
			debug("CodeComponent::getHlView() begin");
			auto result = std::make_unique<::view::HlComponent>();
			result->mutable_code_component()->set_content(this->content);
			result->mutable_code_component()->mutable_hl_tags()->Add(
				this->tags.begin(), this->tags.end()
			);
			result->mutable_code_component()->mutable_assoc_side_infos()->Add(
				this->assoc_side_entries.begin(), this->assoc_side_entries.end()
			);
			auto left = line_suffix_data_t<::view::HlComponent>{ std::move(result) };
			auto mid  = std::vector<line_data_t<::view::HlComponent>>{};
			debug("CodeComponent::getHlView() end");
			return { std::move(left), std::move(mid) };
		}

		component_get_view_data_t<::view::NoHlComponent> CodeComponent::getNoHlView() const {
			debug("CodeComponent::getNoHlView() begin");
			auto result = std::make_unique<::view::NoHlComponent>();
			result->mutable_code_component()->set_content(this->content);
			result->mutable_code_component()->mutable_assoc_side_entries()->Add(
				this->assoc_side_entries.begin(), this->assoc_side_entries.end()
			);
			auto left = line_suffix_data_t<::view::NoHlComponent>{ std::move(result) };
			auto mid  = std::vector<line_data_t<::view::NoHlComponent>>{};
			debug("CodeComponent::getNoHlView() end");
			return { std::move(left), std::move(mid) };
		}

		std::shared_ptr<Component> CodeComponent::deepCopy() {
			auto result = std::make_shared<CodeComponent>(
				this->view_constructor, this->content, this->tags, this->assoc_side_entries
			);
			result->parent = this->parent;
			return std::static_pointer_cast<Component>(result);
		}

		void CodeComponent::registerInteraction(
			InteractionType interaction_type, InteractionContext& interaction_context
		) {
			if (interaction_type == InteractionType::Click)
				openSideEntries(this->assoc_side_entries, this->view_constructor, interaction_context);
			else
				Component::registerInteraction(interaction_type, interaction_context);
		}

		// ConcatComponent

		ConcatComponent::ConcatComponent(
            std::weak_ptr<ViewConstructor> view_constructor,
            std::vector<std::shared_ptr<Component>> components):
			  Component(std::move(view_constructor)),
			  components(std::move(components)) {}

		template<class T>
		component_get_view_data_t<T> concatGetViewHelper(
			std::vector<component_get_view_data_t<T>> subcomponents_results
		) {
			if (ssize(subcomponents_results) == 1) return std::move(subcomponents_results.back());
			std::vector<std::vector<line_data_t<T>>> results_by_lines(1);
			for (auto& [suffix, mid]: subcomponents_results) {
				if (suffix.has_value()) {
					results_by_lines.back().emplace_back(
						line_metadata_t{}, std::move(suffix.value())
					);
				}
				debug("subcomponent mid: ", ssize(mid));
				for (auto& line: mid) {
					std::vector<line_data_t<T>> tmp;
					tmp.emplace_back(std::move(line));
					results_by_lines.emplace_back(std::move(tmp));
				}
			}
			auto wrap_line = [](std::vector<line_data_t<T>>& line) {
				assert(line.empty() == false);
				auto                            line_number = line[0].first;
				std::vector<std::unique_ptr<T>> non_empty;
				for (auto& subcomponent: line)
					if (subcomponent.second.has_value())
						non_empty.emplace_back(std::move(subcomponent.second.value()));
				if (non_empty.empty()) {
					return line_data_t<T>{ line_number, std::optional<std::unique_ptr<T>>() };
				} else {
					auto component = std::make_unique<T>();
					for (auto& subcomponent: non_empty) {
						component->mutable_concat_component()->mutable_components()->AddAllocated(
							std::move(subcomponent).release()
						);
					}
					return line_data_t<T>{ line_number, std::move(component) };
				}
			};
			auto suffix = [wrap_line, &results_by_lines]() {
				auto elm = std::move(results_by_lines[0]);
				results_by_lines.erase(results_by_lines.begin());
				if (elm.empty()) {
					return std::optional<std::unique_ptr<T>>{};
				} else {
					auto [_, component] = wrap_line(elm);
					return std::move(component);
				}
			}();
			std::vector<line_data_t<T>> wrapped;
			for (auto& line: results_by_lines) wrapped.emplace_back(wrap_line(line));
			debug(ssize(wrapped));
			return { std::move(suffix), std::move(wrapped) };
		}

		component_get_view_data_t<::view::HlComponent> ConcatComponent::getHlView() const {
			debug("ConcatComponent::getHlView() begin");
			if (components.empty()) {
				debug("ConcatComponent::getHlView() end");
				return component_get_view_data_t<::view::HlComponent>{};
			}
			auto subcomponents_results
				= std::ranges::subrange(this->components.begin(), this->components.end())
			    | std::views::transform([](const std::shared_ptr<Component>& component) {
					  assert(component);
					  return component->getHlView();
				  });
			auto result = concatGetViewHelper<::view::HlComponent>(
				std::vector(subcomponents_results.begin(), subcomponents_results.end())
			);
			debug("ConcatComponent::getHlView() end");
			return result;
		}

		component_get_view_data_t<::view::NoHlComponent> ConcatComponent::getNoHlView() const {
			if (components.empty()) {
				debug("ConcatComponent::getNoHlView() end");
				return component_get_view_data_t<::view::NoHlComponent>{};
			}
			auto subcomponents_results
				= std::ranges::subrange(this->components.begin(), this->components.end())
			    | std::views::transform([](const std::shared_ptr<Component>& component) {
					  assert(component);
					  return component->getNoHlView();
				  });
			debug("ConcatComponent::getNoHlView() begin");
			auto result = concatGetViewHelper<::view::NoHlComponent>(
				std::vector(subcomponents_results.begin(), subcomponents_results.end())
			);
			debug("ConcatComponent::getNoHlView() end");
			return result;
		}

		std::shared_ptr<Component> ConcatComponent::deepCopy() {
			std::vector<std::shared_ptr<Component>> new_components(ssize(this->components));
			std::transform(
				this->components.begin(),
				this->components.end(),
				new_components.begin(),
				[](const std::shared_ptr<Component>& component) { return component->deepCopy(); }
			);
			auto result = std::make_shared<ConcatComponent>(this->view_constructor, new_components);
			result->parent = this->parent;
			return std::static_pointer_cast<Component>(result);
		}

		// InteractiveComponent

		component_id_t InteractiveComponent::getId() { return this->id; }

		InteractiveComponent::InteractiveComponent(
            std::weak_ptr<ViewConstructor> view_constructor,
			component_id_t                                         id,
			const std::shared_ptr<Component>&                      primary,
			const std::shared_ptr<Component>&                      alternative,
			std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component
		):
			  Component(std::move(view_constructor)),
			  id(id),
			  visible(primary->deepCopy()),
			  primary(primary),
			  alternative(alternative),
			  id_to_interactive_component(std::move(id_to_interactive_component)) {}

		::view::VisibilityStatus toProtocol(const InteractiveComponent::Status& status) {
			switch (status) {
			case InteractiveComponent::Status::Primary:
				return ::view::VisibilityStatus::Primary;
			case InteractiveComponent::Status::Alternative:
				return ::view::VisibilityStatus::Alternative;
			}
		}

		template<class T>
		component_get_view_data_t<T> interactiveGetViewHelper(
			const component_id_t&               id,
			component_get_view_data_t<T>        visible_result,
			const InteractiveComponent::Status& status
		) {
			auto& [suffix, mid]      = visible_result;
			auto wrap_in_interactive = [id, status](std::unique_ptr<T>& to_wrap) {
				auto result = std::make_unique<T>();
				result->mutable_interactive_component()->set_component_id(id);
				result->mutable_interactive_component()->set_allocated_primary_component(
					std::move(to_wrap).release()
				);
				result->mutable_interactive_component()->set_status(toProtocol(status));
				return result;
			};
			if (suffix.has_value()) suffix = wrap_in_interactive(suffix.value());
			transform(
				mid.begin(),
				mid.end(),
				mid.begin(),
				[wrap_in_interactive](line_data_t<T>& line_data) {
					auto& [line_number, component] = line_data;
					if (component.has_value())
						return line_data_t<T>{ line_number, wrap_in_interactive(component.value()) };
					else
						return std::move(line_data);
				}
			);
			return { std::move(suffix), std::move(mid) };
		}

		component_get_view_data_t<::view::HlComponent> InteractiveComponent::getHlView() const {
			debug("InteractiveComponent::getHlView() begin");
			auto result = interactiveGetViewHelper<::view::HlComponent>(
				this->id, this->visible->getHlView(), this->status
			);
			debug("InteractiveComponent::getHlView() end");
			return result;
		}

		component_get_view_data_t<::view::NoHlComponent> InteractiveComponent::getNoHlView() const {
			debug("InteractiveComponent::getNoHlView() begin");
			auto result = interactiveGetViewHelper<::view::NoHlComponent>(
				this->id, this->visible->getNoHlView(), this->status
			);
			debug("InteractiveComponent::getNoHlView() end");
			return result;
		}

		std::shared_ptr<Component> InteractiveComponent::deepCopy() {
			auto result = std::make_shared<InteractiveComponent>(
				this->view_constructor, getNewId(), this->primary, this->alternative, this->id_to_interactive_component
			);
			result->parent = this->parent;
			this->id_to_interactive_component->emplace(
				result->getId(), std::weak_ptr<InteractiveComponent>(result)
			);
			return std::static_pointer_cast<Component>(result);
		}

		void InteractiveComponent::registerInteraction(
			InteractionType interaction_type, InteractionContext& interaction_context
		) {
			debug("InteractiveComponent::registerInteraction begin");
			debug(print(interaction_type));
			if (this->status == Status::Primary
			    && interaction_type == InteractionType::ClickInteractive) {
				debug("Switching to alternative content");
				this->visible = alternative->deepCopy();
				this->status  = Status::Alternative;
			} else if (this->status == Status::Alternative
			           && interaction_type == InteractionType::ClickInteractiveRollback) {
				debug("Switching to primary content");
				this->visible = primary->deepCopy();
				this->status  = Status::Primary;
			} else {
				Component::registerInteraction(interaction_type, interaction_context);
			}
			debug("InteractiveComponent::registerInteraction end");
		}

		// StartLineComponent

		StartLineComponent::StartLineComponent(
            std::weak_ptr<ViewConstructor> view_constructor,std::optional<uint> number):
			  Component(std::move(view_constructor)), number(number) {}

		component_get_view_data_t<::view::HlComponent> StartLineComponent::getHlView() const {
			debug("StartLineComponent::getHlView() begin");
			auto left = line_suffix_data_t<::view::HlComponent>{};
			auto mid  = std::vector<line_data_t<::view::HlComponent>>();
			mid.emplace_back(this->number, std::optional<std::unique_ptr<::view::HlComponent>>{});
			debug("StartLineComponent::getHlView() end");
			return { std::move(left), std::move(mid) };
		}

		component_get_view_data_t<::view::NoHlComponent> StartLineComponent::getNoHlView() const {
			debug("StartLineComponent::getNoHlView() begin");
			auto left = line_suffix_data_t<::view::NoHlComponent>{};
			auto mid  = std::vector<line_data_t<::view::NoHlComponent>>();
			mid.emplace_back(this->number, std::optional<std::unique_ptr<::view::NoHlComponent>>{});
			debug("StartLineComponent::getNoHlView() end");
			return { std::move(left), std::move(mid) };
		}

		std::shared_ptr<Component> StartLineComponent::deepCopy() {
			auto result = std::make_shared<StartLineComponent>(this->view_constructor, this->number);
			result->parent = this->parent;
			return std::static_pointer_cast<Component>(result);
		}

		// Custom

		CreationContext::CreationContext(
			std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component,
			std::shared_ptr<id_to_view_constructor_mapping_t>      id_to_view_constructor,
			std::unique_ptr<std::map<std::string, hl_id_t>>        hl_name_to_id,
			std::optional<DataHandle>                              data_handle,
			std::weak_ptr<ViewConstructor>                         view_constructor
		) : 
			id_to_interactive_component(std::move(id_to_interactive_component)),
			id_to_view_constructor(std::move(id_to_view_constructor)),
			hl_name_to_id(std::move(hl_name_to_id)),
			data_handle(std::move(data_handle)),
			view_constructor(std::move(view_constructor)) {}

		std::unique_ptr<::view::NoHlComponent> concatNoHlLines(
			std::vector<std::unique_ptr<::view::NoHlComponent>> lines
		) {
			auto result = std::make_unique<::view::NoHlComponent>();
			for (auto& line: lines) {
				result->mutable_concat_component()->mutable_components()->AddAllocated(line.release(
				));
			}
			return result;
		}
	}  // namespace view_manager
}  // namespace dia_app
