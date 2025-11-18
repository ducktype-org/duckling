#include "components.hpp"

#include "utils.hpp"

#include <proto/view.pb.h>

#include <memory>

namespace dia_app {
	namespace view_manager {
		component_get_view_data_t<::view::HlComponent> GetHlViewVisitor::visitCodeComponent(
			const CodeComponent& component
		) {
			// Build a single highlighted code component.
			base::Box<::view::HlComponent> hl_component_box = makeBox<::view::HlComponent>();
			hl_component_box->mutable_code_component()->set_content(component.content);
			hl_component_box->mutable_code_component()->mutable_hl_tags()->Add(
				component.tags.begin(), component.tags.end()
			);
			// If there are associated side infos, expose component_id for interactions.
			if (!component.assoc_side_entries.empty())
				hl_component_box->mutable_code_component()->set_component_id(component.id);
			line_suffix_data_t<::view::HlComponent> suffix_component{ std::move(hl_component_box) };
			std::vector<line_data_t<::view::HlComponent>> mid_lines;
			return { std::move(suffix_component), std::move(mid_lines) };
		}

		// Helper to wrap a single HL line into a concat component
		static base::Optional<base::Box<::view::HlComponent>> wrapLineToHlConcat(
			std::vector<line_data_t<::view::HlComponent>>& line
		) {
			// TODO: Review this code
			if (line.empty()) return {};
			std::vector<base::Box<::view::HlComponent>> non_empty_chunks;
			for (auto& [ignored_meta, component_opt]: line)
				if (component_opt.has_value())
					non_empty_chunks.emplace_back(std::move(component_opt.value()));
			if (non_empty_chunks.empty()) return {};
			base::Box<::view::HlComponent> concat_component_box = makeBox<::view::HlComponent>();
			auto*                          repeated_components
				= concat_component_box->mutable_concat_component()->mutable_components();
			for (auto& chunk_box: non_empty_chunks) {
				auto* added = repeated_components->Add();
				added->Swap(&*chunk_box);
			}
			return concat_component_box;
		}

		component_get_view_data_t<::view::HlComponent> GetHlViewVisitor::visitConcatComponent(
			const ConcatComponent& component
		) {
			// TODO: Review this code
			std::vector<std::vector<line_data_t<::view::HlComponent>>> results_by_lines(1);
			for (const auto& child_component: component.components) {
				auto [child_suffix, child_mid] = child_component->accept(*this);
				if (child_suffix.has_value())
					results_by_lines.back().emplace_back(
						line_metadata_t{}, std::move(child_suffix.value())
					);
				for (auto& line_data: child_mid) {
					std::vector<line_data_t<::view::HlComponent>> single_line;
					single_line.emplace_back(std::move(line_data));
					results_by_lines.emplace_back(std::move(single_line));
				}
			}
			line_suffix_data_t<::view::HlComponent> suffix_component
				= wrapLineToHlConcat(results_by_lines.front());
			results_by_lines.erase(results_by_lines.begin());
			std::vector<line_data_t<::view::HlComponent>> wrapped_lines;
			wrapped_lines.reserve(results_by_lines.size());
			for (auto& grouped_line: results_by_lines) {
				line_metadata_t line_meta
					= grouped_line.empty() ? line_metadata_t{} : grouped_line[0].first;
				wrapped_lines.emplace_back(line_meta, wrapLineToHlConcat(grouped_line));
			}
			return { std::move(suffix_component), std::move(wrapped_lines) };
		}

		component_get_view_data_t<::view::HlComponent> GetHlViewVisitor::visitInteractiveComponent(
			const InteractiveComponent& component
		) {
			auto [visible_suffix, visible_mid] = component.primary->accept(*this);
			auto wrap_in_interactive = [&](base::Optional<base::Box<::view::HlComponent>> son
			                           ) -> base::Optional<base::Box<::view::HlComponent>> {
				if (son.has_value()) {
					auto  result            = makeBox<::view::HlComponent>();
					auto* interactive_proto = result->mutable_interactive_component();
					interactive_proto->set_component_id(component.id);
					interactive_proto->set_status(
						component.status == InteractiveComponent::Status::Primary
							? ::view::VisibilityStatus::Primary
							: ::view::VisibilityStatus::Alternative
					);
					// Need a releasable owner for protobuf adoption
					base::Box<::view::HlComponent> primary_ptr = makeBox<::view::HlComponent>();
					primary_ptr->Swap(&*son.value());
					interactive_proto->set_allocated_primary_component(primary_ptr.release());
					return result;
				} else {
					return {};
				}
			};
			std::vector<line_data_t<::view::HlComponent>> result;
			for (auto& mid: visible_mid)
				result.emplace_back(mid.first, wrap_in_interactive(std::move(mid.second)));
			return { wrap_in_interactive(std::move(visible_suffix)), std::move(result) };
		}

		component_get_view_data_t<::view::HlComponent> GetHlViewVisitor::visitStartLineComponent(
			const StartLineComponent& component
		) {
			std::vector<line_data_t<::view::HlComponent>> mid_lines;
			mid_lines.emplace_back(component.number, line_suffix_data_t<::view::HlComponent>{});
			return { line_suffix_data_t<::view::HlComponent>{}, std::move(mid_lines) };
		}

		component_get_view_data_t<::view::NoHlComponent> GetNoHlViewVisitor::visitTextComponent(
			const TextComponent& component
		) {
			base::Box<::view::NoHlComponent> nohl_component_box = makeBox<::view::NoHlComponent>();
			nohl_component_box->mutable_text_component()->set_content(component.content);
			// If there are associated side infos, expose component_id for interactions.
			if (!component.assoc_side_entries.empty())
				nohl_component_box->mutable_text_component()->set_component_id(component.id);
			return { line_suffix_data_t<::view::NoHlComponent>{ std::move(nohl_component_box) },
				     std::vector<line_data_t<::view::NoHlComponent>>{} };
		}

		component_get_view_data_t<::view::NoHlComponent> GetNoHlViewVisitor::visitCodeComponent(
			const CodeComponent& component
		) {
			base::Box<::view::NoHlComponent> nohl_component_box = makeBox<::view::NoHlComponent>();
			nohl_component_box->mutable_code_component()->set_content(component.content);
			// If there are associated side infos, expose component_id for interactions.
			if (!component.assoc_side_entries.empty())
				nohl_component_box->mutable_code_component()->set_component_id(component.id);
			return { line_suffix_data_t<::view::NoHlComponent>{ std::move(nohl_component_box) },
				     std::vector<line_data_t<::view::NoHlComponent>>{} };
		}

		// Helper to wrap a single no-highlight line into a concat
		static base::Optional<base::Box<::view::NoHlComponent>> wrapLineToNoHlConcat(
			std::vector<line_data_t<::view::NoHlComponent>>& line
		) {
			// TODO: Review this code
			if (line.empty()) return {};
			std::vector<base::Box<::view::NoHlComponent>> non_empty_chunks;
			for (auto& [ignored_meta, component_opt]: line)
				if (component_opt.has_value())
					non_empty_chunks.emplace_back(std::move(component_opt.value()));
			if (non_empty_chunks.empty()) return {};
			base::Box<::view::NoHlComponent> concat_component_box
				= makeBox<::view::NoHlComponent>();
			auto* repeated_components
				= concat_component_box->mutable_concat_component()->mutable_components();
			for (auto& chunk_box: non_empty_chunks) {
				auto* added = repeated_components->Add();
				added->Swap(&*chunk_box);
			}
			return concat_component_box;
		}

		component_get_view_data_t<::view::NoHlComponent> GetNoHlViewVisitor::visitConcatComponent(
			const ConcatComponent& component
		) {
			// TODO: Review this code
			std::vector<std::vector<line_data_t<::view::NoHlComponent>>> results_by_lines(1);
			for (const auto& child_component: component.components) {
				auto [child_suffix, child_mid] = child_component->accept(*this);
				if (child_suffix.has_value())
					results_by_lines.back().emplace_back(
						line_metadata_t{}, std::move(child_suffix.value())
					);
				for (auto& line_data: child_mid) {
					std::vector<line_data_t<::view::NoHlComponent>> single_line;
					single_line.emplace_back(std::move(line_data));
					results_by_lines.emplace_back(std::move(single_line));
				}
			}
			line_suffix_data_t<::view::NoHlComponent> suffix_component
				= wrapLineToNoHlConcat(results_by_lines.front());
			results_by_lines.erase(results_by_lines.begin());
			std::vector<line_data_t<::view::NoHlComponent>> wrapped_lines;
			wrapped_lines.reserve(results_by_lines.size());
			for (auto& grouped_line: results_by_lines) {
				line_metadata_t line_meta
					= grouped_line.empty() ? line_metadata_t{} : grouped_line[0].first;
				wrapped_lines.emplace_back(line_meta, wrapLineToNoHlConcat(grouped_line));
			}
			return { std::move(suffix_component), std::move(wrapped_lines) };
		}

		component_get_view_data_t<::view::NoHlComponent> GetNoHlViewVisitor::visitInteractiveComponent(
			const InteractiveComponent& component
		) {
			auto [visible_suffix, visible_mid] = component.primary->accept(*this);
			auto wrap_in_interactive = [&](base::Optional<base::Box<::view::NoHlComponent>> son
			                           ) -> base::Optional<base::Box<::view::NoHlComponent>> {
				if (son.has_value()) {
					auto  result            = makeBox<::view::NoHlComponent>();
					auto* interactive_proto = result->mutable_interactive_component();
					interactive_proto->set_component_id(component.id);
					interactive_proto->set_status(
						component.status == InteractiveComponent::Status::Primary
							? ::view::VisibilityStatus::Primary
							: ::view::VisibilityStatus::Alternative
					);
					// Need a releasable owner for protobuf adoption
					base::Box<::view::NoHlComponent> primary_ptr = makeBox<::view::NoHlComponent>();
					primary_ptr->Swap(&*son.value());
					interactive_proto->set_allocated_primary_component(primary_ptr.release());
					return result;
				} else {
					return {};
				}
			};
			std::vector<line_data_t<::view::NoHlComponent>> result;
			for (auto& mid: visible_mid)
				result.emplace_back(mid.first, wrap_in_interactive(std::move(mid.second)));
			return { wrap_in_interactive(std::move(visible_suffix)), std::move(result) };
		}

		component_get_view_data_t<::view::NoHlComponent> GetNoHlViewVisitor::visitStartLineComponent(
			const StartLineComponent& component
		) {
			std::vector<line_data_t<::view::NoHlComponent>> mid_lines;
			mid_lines.emplace_back(component.number, line_suffix_data_t<::view::NoHlComponent>{});
			return { line_suffix_data_t<::view::NoHlComponent>{}, std::move(mid_lines) };
		}

		void InteractionVisitor::visitTextComponent(TextComponent& component) {
			// TODO: Do something with side entries for side panel
			auto parent = component.parent->lock();
			if (parent) parent->accept(*this);
		}

		void InteractionVisitor::visitCodeComponent(CodeComponent& component) {
			// TODO: Do something with side entries for side panel
			auto parent = component.parent->lock();
			if (parent) parent->accept(*this);
		}

		void InteractionVisitor::visitConcatComponent(ConcatComponent& component) {
			auto parent = component.parent->lock();
			if (parent) parent->accept(*this);
		}

		void InteractionVisitor::visitInteractiveComponent(InteractiveComponent& component) {
			std::swap(component.primary, component.alternative);
			component.primary->reset();
			if (component.status == InteractiveComponent::Status::Primary)
				component.status = InteractiveComponent::Status::Alternative;
			else
				component.status = InteractiveComponent::Status::Primary;
		}

		void InteractionVisitor::visitStartLineComponent(StartLineComponent& component) {
			auto parent = component.parent->lock();
			if (parent) parent->accept(*this);
		}

		// Component
		Component::Component(base::Optional<std::weak_ptr<Component>> parent):
			  parent(std::move(parent)) {}

		Component::~Component() {}

		void Component::setParent(const std::weak_ptr<Component>& p) { this->parent = p; }

		// TextComponent

		TextComponent::TextComponent(
			ComponentID id, std::string content, std::vector<side_entry_id_t> assoc_side_entries
		):
			  Component(),
			  id(id),
			  content(std::move(content)),
			  assoc_side_entries(std::move(assoc_side_entries)) {}

		component_get_view_data_t<::view::HlComponent> TextComponent::accept(GetHlViewVisitor& visitor
		) const {
			assert(false);
		}

		component_get_view_data_t<::view::NoHlComponent> TextComponent::accept(
			GetNoHlViewVisitor& visitor
		) const {
			return visitor.visitTextComponent(*this);
		}

		void TextComponent::accept(InteractionVisitor& visitor) {
			visitor.visitTextComponent(*this);
		}

		void TextComponent::reset() {}

		// CodeComponent

		CodeComponent::CodeComponent(
			ComponentID               id,
			std::string                  content,
			std::vector<PointerMessageID>         tags,
			std::vector<side_entry_id_t> assoc_side_entries
		):
			  Component(),
			  id(id),
			  content(std::move(content)),
			  tags(std::move(tags)),
			  assoc_side_entries(std::move(assoc_side_entries)) {}

		component_get_view_data_t<::view::HlComponent> CodeComponent::accept(GetHlViewVisitor& visitor
		) const {
			return visitor.visitCodeComponent(*this);
		}

		component_get_view_data_t<::view::NoHlComponent> CodeComponent::accept(
			GetNoHlViewVisitor& visitor
		) const {
			return visitor.visitCodeComponent(*this);
		}

		void CodeComponent::accept(InteractionVisitor& visitor) {
			visitor.visitCodeComponent(*this);
		}

		void CodeComponent::reset() {}

		// ConcatComponent

		ConcatComponent::ConcatComponent(std::vector<std::shared_ptr<Component>> components):
			  Component(),
			  components(std::move(components)) {}

		component_get_view_data_t<::view::HlComponent> ConcatComponent::accept(
			GetHlViewVisitor& visitor
		) const {
			return visitor.visitConcatComponent(*this);
		}

		component_get_view_data_t<::view::NoHlComponent> ConcatComponent::accept(
			GetNoHlViewVisitor& visitor
		) const {
			return visitor.visitConcatComponent(*this);
		}

		void ConcatComponent::accept(InteractionVisitor& visitor) {
			visitor.visitConcatComponent(*this);
		}

		void ConcatComponent::reset() {
			for (auto& component: this->components)
				if (component) component->reset();
		}

		// InteractiveComponent

		InteractiveComponent::InteractiveComponent(
			ComponentID                    id,
			const std::shared_ptr<Component>& primary,
			const std::shared_ptr<Component>& alternative
		):
			  Component(),
			  id(id),
			  primary(primary),
			  alternative(alternative) {}

		component_get_view_data_t<::view::HlComponent> InteractiveComponent::accept(
			GetHlViewVisitor& visitor
		) const {
			return visitor.visitInteractiveComponent(*this);
		}

		component_get_view_data_t<::view::NoHlComponent> InteractiveComponent::accept(
			GetNoHlViewVisitor& visitor
		) const {
			return visitor.visitInteractiveComponent(*this);
		}

		void InteractiveComponent::accept(InteractionVisitor& visitor) {
			visitor.visitInteractiveComponent(*this);
		}

		void InteractiveComponent::reset() {
			if (this->status == Status::Alternative) {
				swap(this->primary, this->alternative);
				this->status = Status::Primary;
			}
			if (this->primary) this->primary->reset();
			if (this->alternative) this->alternative->reset();
		}

		// StartLineComponent

		StartLineComponent::StartLineComponent(base::Optional<uint> number):
			  Component(),
			  number(number) {}

		component_get_view_data_t<::view::HlComponent> StartLineComponent::accept(
			GetHlViewVisitor& visitor
		) const {
			return visitor.visitStartLineComponent(*this);
		}

		component_get_view_data_t<::view::NoHlComponent> StartLineComponent::accept(
			GetNoHlViewVisitor& visitor
		) const {
			return visitor.visitStartLineComponent(*this);
		}

		void StartLineComponent::accept(InteractionVisitor& visitor) {
			visitor.visitStartLineComponent(*this);
		}

		void StartLineComponent::reset() {}
	}  // namespace view_manager
}  // namespace dia_app
