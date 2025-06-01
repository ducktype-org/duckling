#include "display_elements.hpp"

#include "components.hpp"

namespace dia_app {
	namespace dia_file {
		using view_manager::Component, view_manager::InteractiveComponent,
			view_manager::ConcatComponent, view_manager::TextComponent, view_manager::CodeComponent,
			view_manager::StartLineComponent, view_manager::CreationContext, view_manager::hl_id_t;

		Ptr                  fetch_resource(ResourceHandle, DataHandle data_handle);
		void                 fetch_entity(EntityHandle entity_handle, DataHandle data_handle);
		std::set<InfoHandle> scan_entity_metadata(const json& entity, DataHandle handle);

		// ---------------- TextElement ---------------- //

		TextElement::TextElement(const json& elem_json) {
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

		TextElement::TextElement(const TextElement& other):
			  DisplayElement(other),
			  content(other.content) {}

		Ptr TextElement::copy() const { return std::make_shared<TextElement>(*this); }

		std::string TextElement::toText(DataHandle _) const { return content; }

		json TextElement::show(DataHandle _) const {
			json res;
			res["type"]    = "text";
			res["content"] = content;
			res["groups"]  = groups;
			return res;
		}

		std::shared_ptr<Component> TextElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(acc_data);
			// This element involves no computations,
			// so caching can be skipped.
			return std::make_shared<TextComponent>(creation_context->view_constructor, this->content, acc_data.getAssocInfos());
		}

		// ---------------- ConcatElement ---------------- //

		ConcatElement::ConcatElement(const json& elem_json) {
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

		ConcatElement::ConcatElement(const ConcatElement& other): DisplayElement(other) {
			for (auto& elem: other.elems) elems.push_back(elem->copy());
		}

		ConcatElement::ConcatElement(const std::vector<Ptr>& elems): elems(elems) {}

		Ptr ConcatElement::copy() const { return std::make_shared<ConcatElement>(*this); }

		std::string ConcatElement::toText(DataHandle dh) const {
			std::string res;
			for (auto& elem: elems) res += elem->toText(dh);
			return res;
		}

		json ConcatElement::show(DataHandle dh) const {
			json res;
			for (auto& el: elems) res.push_back(el->show(dh));
			return res;
		}

		std::shared_ptr<Component> ConcatElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(acc_data);
			if (!this->generated_component) {
				std::vector<std::shared_ptr<Component>> sons(ssize(this->elems));
				transform(
					this->elems.begin(),
					this->elems.end(),
					sons.begin(),
					[&creation_context, &acc_data](const Ptr& son) {
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
				this->generated_component
					= static_pointer_cast<Component>(component);
			}
			return this->generated_component;
		}

		// ---------------- StartLineElement ---------------- //

		StartLineElement::StartLineElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "start_line");
			if (elem_json.contains("number")) ASSUME_HAS_UINT_ASSIGN(elem_json, number);
		}

		StartLineElement::StartLineElement(const StartLineElement& other):
			  DisplayElement(other),
			  number(other.number) {}

		Ptr StartLineElement::copy() const { return std::make_shared<StartLineElement>(*this); }

		std::string StartLineElement::toText(DataHandle dh) const { return "\n"; }

		json StartLineElement::show(DataHandle dh) const {
			json res;
			res["type"] = "start_line";
			if (number.has_value()) res["number"] = number.value();
			return res;
		}

		std::shared_ptr<Component> StartLineElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			// This element involves no computations,
			// so caching can be skipped.
			// Moreover, start line components ignore assoc infos
			// and groups.
			return make_shared<StartLineComponent>(creation_context->view_constructor, this->number);
		}

		// ---------------- InteractElement ---------------- //

		InteractElement::InteractElement(const json& elem_json) {
			// Both "entity" and "grouping" syntax elements can be
			// interactive.
			// Note that entity data is lost when creating an instance
			// of InteractElement - that is why entities are recognised
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

		InteractElement::InteractElement(const InteractElement& other):
			  DisplayElement(other),
			  content(other.content->copy()),
			  alt_content(other.alt_content->copy()) {}

		Ptr InteractElement::copy() const { return std::make_shared<InteractElement>(*this); }

		std::string InteractElement::toText(DataHandle dh) const { return content->toText(dh); }

		json InteractElement::show(DataHandle dh) const {
			json res;
			res["type"]        = "interact";
			res["content"]     = content->show(dh);
			res["alt_content"] = alt_content->show(dh);
			return res;
		}

		std::shared_ptr<Component> InteractElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(acc_data);
			if (!this->generated_component) {
				auto primary     = this->content->toComponentImpl(creation_context, acc_data);
				auto alternative = this->alt_content->toComponentImpl(creation_context, acc_data);
				auto result      = make_shared<InteractiveComponent>(
                    creation_context->view_constructor,
                    view_manager::getNewId(),
                    primary,
                    alternative,
                    creation_context->id_to_interactive_component
                );
                primary->parent = result;
                alternative->parent = result;
				creation_context->id_to_interactive_component->emplace(
					result->getId(), std::weak_ptr<InteractiveComponent>(result)
				);
				this->generated_component = static_pointer_cast<Component>(result);
			}
			return this->generated_component;
		}

		// ---------------- LazyElement ---------------- //

		LazyElement::LazyElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "lazy");
			ASSUME_HAS(elem_json, "handle");
			handle = elem_json["handle"];
		}

		LazyElement::LazyElement(const LazyElement& other):
			  DisplayElement(other),
			  handle(other.handle) {}

		Ptr LazyElement::copy() const { return std::make_shared<LazyElement>(*this); }

		std::string LazyElement::toText(DataHandle dh) const { return evaluated(dh)->toText(dh); }

		Ptr LazyElement::evaluated(DataHandle dh) const {
			Ptr res = fetch_resource(handle, dh);
			// Associated infos and groups are passed onto the newly fetched
			// resource.
			res->assoc_infos.insert(assoc_infos.begin(), assoc_infos.end());
			res->groups.insert(groups.begin(), groups.end());
			return res;
		}

		json LazyElement::show(DataHandle dh) const { return evaluated(dh)->show(dh); }

		std::shared_ptr<Component> LazyElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(acc_data);
			// This element does not correspond to any component in the result,
			// thus the caching can be skipped.
			return evaluated(creation_context->data_handle.value())
			    ->toComponentImpl(creation_context, acc_data);
		}

		// ---------------- EntityElement ---------------- //

		EntityElement::EntityElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "entity");
			ASSUME_HAS_STR_ASSIGN(elem_json, refers_to);

			// Entities may be interactive in the diagnostics
			// file syntax.
			if (elem_json.contains("alt_content")) {
				content = std::make_shared<InteractElement>(elem_json);
			} else {
				ASSUME_HAS(elem_json, "content");
				content = parse(elem_json["content"]);

				if (elem_json.contains("groups")) {
					ASSUME_ARR(elem_json, "groups");
					groups = elem_json["groups"];
				}
			}
		}

		EntityElement::EntityElement(const EntityElement& other):
			  DisplayElement(other),
			  refers_to(other.refers_to),
			  content(other.content->copy()) {}

		Ptr EntityElement::copy() const { return std::make_shared<EntityElement>(*this); }

		std::set<InfoHandle> EntityElement::getAssocInfos(DataHandle dh) const {
			auto res = assoc_infos;
			// Scan the entity metadata for associated infos
			// and fetch all the necessary resources along the way.
			ASSUME_HAS(dh.entities, refers_to);
			ASSUME_HAS_STR(dh.entities[refers_to], "kind");
			auto& entity = dh.entities[refers_to];

			// If the referred entity is not fetched, do it now.
			if (entity["kind"] == "lazy_entity") {
				ASSUME_HAS(entity, "handle");
				fetch_entity({ refers_to, entity["handle"] }, dh);
			}
			// Scan the entity's metadata for associated infos.
			auto entity_res = scan_entity_metadata(dh.entities[refers_to], dh);

			// Merge the two info pools.
			res.insert(entity_res.begin(), entity_res.end());
			return res;
		}

		std::string EntityElement::toText(DataHandle dh) const { return content->toText(dh); }

		json EntityElement::show(DataHandle dh) const {
			json res;
			res["type"]      = "entity";
			res["refers_to"] = refers_to;
			res["content"]   = content->show(dh);
			return res;
		}

		std::shared_ptr<view_manager::Component> EntityElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(acc_data);
			// This element does not correspond to any component in the result,
			// thus the caching can be skipped.
			return this->content->toComponentImpl(creation_context, acc_data);
		}

		// ---------------- CodeElement ---------------- //

		CodeElement::CodeElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "code");
			ASSUME_HAS_STR(elem_json, "content");
			content = elem_json["content"];

			if (elem_json.contains("groups")) {
				ASSUME_ARR(elem_json, "groups");
				groups = elem_json["groups"];
			}
		}

		CodeElement::CodeElement(const CodeElement& other):
			  DisplayElement(other),
			  content(other.content) {}

		Ptr CodeElement::copy() const { return std::make_shared<CodeElement>(*this); }

		std::string CodeElement::toText(DataHandle dh) const { return content; }

		json CodeElement::show(DataHandle dh) const {
			json res;
			res["type"]    = "code";
			res["content"] = content;
			return res;
		}

		std::shared_ptr<Component> CodeElement::toComponentImpl(
			std::shared_ptr<CreationContext> creation_context, AccData acc_data
		) {
			accumulateData(acc_data);
			if (!this->generated_component) {
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
				auto result = std::make_shared<CodeComponent>(
					creation_context->view_constructor, this->content, std::move(tags), acc_data.getAssocInfos()
				);
				this->generated_component = std::static_pointer_cast<Component>(result);
			}
			return this->generated_component;
		}

		// ---------------- functions ---------------- //

		/* Order of message type evaluation:
		    lazy -> entity -> interact -> start_line -> code -> concat -> text */
		Ptr parse(const json& msg) {
			if (msg.contains("type") && msg["type"] == "lazy")
				return std::make_shared<LazyElement>(msg);
			if (msg.contains("type") && msg["type"] == "entity")
				return std::make_shared<EntityElement>(msg);
			if (msg.contains("alt_content")) return std::make_shared<InteractElement>(msg);
			if (msg.contains("type") && msg["type"] == "start_line")
				return std::make_shared<StartLineElement>(msg);
			if (msg.contains("type") && msg["type"] == "code")
				return std::make_shared<CodeElement>(msg);
			if (msg.is_array() || (msg.contains("type") && msg["type"] == "grouping"))
				return std::make_shared<ConcatElement>(msg);
			return std::make_shared<TextElement>(msg);
		}

		Ptr fetch_resource(ResourceHandle resource_handle, DataHandle data_handle) {
			// Here fetching from the LS may happen in the future
			// which may modify the data under data_handle.
			return parse(resource_handle);
		}

		void fetch_entity(EntityHandle entity_handle, DataHandle data_handle) {
			// Here fetching from the LS may happen in the future
			// which may modify the data under data_handle.
			data_handle.entities[entity_handle.first] = entity_handle.second;
		}

		std::set<InfoHandle> scan_entity_metadata(const json& entity, DataHandle handle) {
			// Note: during scanning it may be necessary to fetch
			//       some other entities - that's what the data handle is for.
			std::set<InfoHandle> res;

			// Definition scan.
			if (entity.contains("defined_at")) {
				ASSUME_UINT(entity, "defined_at");
				res.insert(InfoParamsHandle::add(entity["defined_at"], handle));
			}
			// TODO: more functionalities may be added here.

			return res;
		}

	}  // namespace dia_file
}  // namespace dia_app
