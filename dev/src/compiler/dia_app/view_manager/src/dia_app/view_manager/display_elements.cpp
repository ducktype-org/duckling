#include "display_elements.hpp"

#include "components.hpp"

namespace dia_app {
	namespace dia_file {
		using view_manager::Component, view_manager::InteractiveComponent,
			view_manager::ConcatComponent, view_manager::TextComponent, view_manager::CodeComponent,
			view_manager::StartLineComponent, view_manager::CreationContext, view_manager::hl_id_t;

		void DisplayElement::accumulateData(std::shared_ptr<view_manager::CreationContext> creation_context, AccData& acc) const {
			// Evaluate all associated infos with this element (there may be more
			// than just in the assoc_infos field).
			auto new_infos = getAssocInfos(creation_context->data_handle.value());

			acc.assoc_infos.insert(new_infos.begin(), new_infos.end());
			acc.groups.insert(groups.begin(), groups.end());
		}

		// ---------------- TextDElement ---------------- //

		TextDElement::TextDElement(const json& elem_json) {
			if (elem_json.is_string()) {
				// Plain text, no metadata.
				content = elem_json;
				return;
			}
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "text");
			ASSUME_HAS_STR_ASSIGN(elem_json, content);

			if (elem_json.contains("groups")) {
				ASSUME_ARR(elem_json, "groups");
				groups = elem_json["groups"];
			}
		}

		TextDElement::TextDElement(const TextDElement& other):
			  DisplayElement(other),
			  content(other.content) {}

		DisplayPtr TextDElement::copy() const { return std::make_shared<TextDElement>(*this); }

		std::shared_ptr<Component> TextDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(creation_context, acc_data);
			// This element involves no computations,
			// so caching can be skipped.
			auto id = view_manager::getNewId();
			auto result = std::make_shared<TextComponent>(id,creation_context->view_constructor, this->content, acc_data.getAssocInfos());
			creation_context->id_to_component->emplace(
				id, std::weak_ptr<Component>(result)
			);
			return result;
		}

		// ---------------- ConcatDElement ---------------- //

		ConcatDElement::ConcatDElement(const json& elem_json) {
			if (elem_json.is_array()) {
				// A simple array of elements.
				for (auto& el: elem_json) elems.push_back(parse(el));
			} else {
				// An element grouping with possible pointer message groups.
				ASSUME_HAS(elem_json, "type");
				ASSUME_VAL(elem_json, "type", "grouping");

				if (elem_json.contains("groups")) {
					ASSUME_ARR(elem_json, "groups");
					groups = elem_json["groups"];
				}

				// We only create thin concat elements from element groupings.
				ASSUME_HAS(elem_json, "content");
				elems.push_back(parse(elem_json["content"]));
			}
		}

		ConcatDElement::ConcatDElement(const ConcatDElement& other): DisplayElement(other) {
			for (auto& elem: other.elems) elems.push_back(elem->copy());
		}

		ConcatDElement::ConcatDElement(const std::vector<DisplayPtr>& elems): elems(elems) {}

		DisplayPtr ConcatDElement::copy() const { return std::make_shared<ConcatDElement>(*this); }

		std::shared_ptr<Component> ConcatDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(creation_context, acc_data);
			std::vector<std::shared_ptr<Component>> sons(ssize(this->elems));
			transform(
				this->elems.begin(),
				this->elems.end(),
				sons.begin(),
				[&creation_context, &acc_data](const DisplayPtr& son) {
					return son->toComponentImpl(creation_context, acc_data);
				}
			);
			sons.erase(
				std::remove_if(
					sons.begin(),
					sons.end(),
					[](const std::shared_ptr<Component>& ptr) { return !ptr; }
				),
				sons.end()
			);
			auto component = make_shared<ConcatComponent>(creation_context->view_constructor, sons);
			for (const auto &son : component->components) {
				son->parent = component;
			}
			return static_pointer_cast<Component>(component);
		}

		// ---------------- StartLineDElement ---------------- //

		StartLineDElement::StartLineDElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "start_line");
			if (elem_json.contains("number")) ASSUME_HAS_UINT_ASSIGN(elem_json, number);
		}

		StartLineDElement::StartLineDElement(const StartLineDElement& other):
			  DisplayElement(other),
			  number(other.number) {}

		DisplayPtr StartLineDElement::copy() const { return std::make_shared<StartLineDElement>(*this); }

		std::shared_ptr<Component> StartLineDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			// This element involves no computations,
			// so caching can be skipped.
			// Moreover, start line components ignore assoc infos
			// and groups.
			return make_shared<StartLineComponent>(creation_context->view_constructor, this->number);
		}

		// ---------------- InteractDElement ---------------- //

		InteractDElement::InteractDElement(const json& elem_json) {
			// Both "entity" and "grouping" syntax elements can be
			// interactive.
			// Note that entity data is lost when creating an instance
			// of InteractDElement - that is why entities are recognised
			// before interacts upon parsing.
			ASSUME_HAS(elem_json, "type");
			ASSUME(
				elem_json["type"] == "entity" || elem_json["type"] == "grouping",
				"interactive element is neither entity nor a grouping"
			);

			ASSUME_HAS(elem_json, "content");
			content = parse(elem_json["content"]);
			ASSUME_HAS(elem_json, "alt_content");
			alt_content = parse(elem_json["alt_content"]);

			if (elem_json.contains("groups")) {
				ASSUME_ARR(elem_json, "groups");
				groups = elem_json["groups"];
			}
		}

		InteractDElement::InteractDElement(const InteractDElement& other):
			  DisplayElement(other),
			  content(other.content->copy()),
			  alt_content(other.alt_content->copy()) {}

		DisplayPtr InteractDElement::copy() const { return std::make_shared<InteractDElement>(*this); }

		std::shared_ptr<Component> InteractDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(creation_context, acc_data);
			auto primary     = this->content->toComponentImpl(creation_context, acc_data);
			auto alternative = this->alt_content->toComponentImpl(creation_context, acc_data);
			auto result      = make_shared<InteractiveComponent>(
				creation_context->view_constructor,
				view_manager::getNewId(),
				primary,
				alternative,
				creation_context->id_to_component
			);
			primary->parent = result;
			alternative->parent = result;
			creation_context->id_to_component->emplace(
				result->getId(), std::weak_ptr<Component>(result)
			);
			return static_pointer_cast<Component>(result);
		}

		// ---------------- LazyDElement ---------------- //

		LazyDElement::LazyDElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "lazy");
			ASSUME_HAS(elem_json, "handle");
			id = elem_json["handle"];
		}

		LazyDElement::LazyDElement(const LazyDElement& other):
			  DisplayElement(other),
			  id(other.id) {}

		DisplayPtr LazyDElement::copy() const { return std::make_shared<LazyDElement>(*this); }

		DisplayPtr LazyDElement::evaluated(ViewConstructor &vc) const {
			return vc.getLazyElement(id);
		}

		std::shared_ptr<Component> LazyDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(creation_context, acc_data);
			// This element does not correspond to any component in the result,
			// thus the caching can be skipped.
			return evaluated(creation_context->data_handle.value())
			    ->toComponentImpl(creation_context, acc_data);
		}

		// ---------------- EntityDElement ---------------- //

		EntityDElement::EntityDElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "entity");
			ASSUME_HAS_STR_ASSIGN(elem_json, refers_to);

			// Entities may be interactive in the diagnostics
			// file syntax.
			if (elem_json.contains("alt_content")) {
				content = std::make_shared<InteractDElement>(elem_json);
			} else {
				ASSUME_HAS(elem_json, "content");
				content = parse(elem_json["content"]);

				if (elem_json.contains("groups")) {
					ASSUME_ARR(elem_json, "groups");
					groups = elem_json["groups"];
				}
			}
		}

		EntityDElement::EntityDElement(const EntityDElement& other):
			  DisplayElement(other),
			  refers_to(other.refers_to),
			  content(other.content->copy()) {}

		DisplayPtr EntityDElement::copy() const { return std::make_shared<EntityDElement>(*this); }

		std::set<InfoID> EntityDElement::getAssocInfos(ViewConstructor &vc) const {
			auto res = assoc_infos;
			auto entity_res = vc.assocInfosFromEntity(refers_to);

			// Merge the two info pools.
			res.insert(entity_res.begin(), entity_res.end());
			return res;
		}

		std::shared_ptr<view_manager::Component> EntityDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(creation_context, acc_data);
			// This element does not correspond to any component in the result,
			// thus the caching can be skipped.
			return this->content->toComponentImpl(creation_context, acc_data);
		}

		// ---------------- CodeDElement ---------------- //

		CodeDElement::CodeDElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "code");
			ASSUME_HAS_STR(elem_json, "content");
			content = elem_json["content"];

			if (elem_json.contains("groups")) {
				ASSUME_ARR(elem_json, "groups");
				groups = elem_json["groups"];
			}
		}

		CodeDElement::CodeDElement(const CodeDElement& other):
			  DisplayElement(other),
			  content(other.content) {}

		DisplayPtr CodeDElement::copy() const { return std::make_shared<CodeDElement>(*this); }

		std::shared_ptr<Component> CodeDElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(creation_context, acc_data);
			auto                 groups = acc_data.getGroups();
			std::vector<hl_id_t> tags(ssize(groups));
			std::transform(
				groups.begin(),
				groups.end(),
				tags.begin(),
				[&creation_context](const std::string& group) {
					return creation_context->hl_name_to_id->at(group);
				}
			);
			auto id = view_manager::getNewId();
			auto result = std::make_shared<CodeComponent>(
				id, creation_context->view_constructor, this->content, std::move(tags), acc_data.getAssocInfos()
			);
			creation_context->id_to_component->emplace(
				id, std::weak_ptr<Component>(result)
			);
			return std::static_pointer_cast<Component>(result);
		}

		// ---------------- functions ---------------- //

		/* Order of message type evaluation:
		    lazy -> entity -> interact -> start_line -> code -> concat -> text */
		DisplayPtr parse(const json& msg) {
			if (msg.contains("type") && msg["type"] == "lazy")
				return std::make_shared<LazyDElement>(msg);
			if (msg.contains("type") && msg["type"] == "entity")
				return std::make_shared<EntityDElement>(msg);
			if (msg.contains("alt_content")) return std::make_shared<InteractDElement>(msg);
			if (msg.contains("type") && msg["type"] == "start_line")
				return std::make_shared<StartLineDElement>(msg);
			if (msg.contains("type") && msg["type"] == "code")
				return std::make_shared<CodeDElement>(msg);
			if (msg.is_array() || (msg.contains("type") && msg["type"] == "grouping"))
				return std::make_shared<ConcatDElement>(msg);
			return std::make_shared<TextDElement>(msg);
		}
	}  // namespace dia_file
}  // namespace dia_app
