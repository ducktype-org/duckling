#include "display_elements.hpp"

#include "components.hpp"

namespace dia_app {
	namespace dia_file {
		using view_manager::Component, view_manager::InteractiveComponent,
			view_manager::ConcatComponent, view_manager::TextComponent, view_manager::CodeComponent,
			view_manager::StartLineComponent, view_manager::hl_id_t;

		DisplayElement::DisplayElement(const DisplayElement& other):
			  assoc_infos(other.assoc_infos),
			  pointer_messages(other.pointer_messages) {}

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
		}

		TextDElement::TextDElement(const TextDElement& other):
			  DisplayElement(other),
			  content(other.content) {}

		DisplayPtr TextDElement::copy() const { return std::make_shared<TextDElement>(*this); }

		std::shared_ptr<view_manager::Component> TextDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitTextDElement(*this);
		}

		// ---------------- ConcatDElement ---------------- //

		ConcatDElement::ConcatDElement(const json& elem_json) {
			if (elem_json.is_array()) {
				// A simple array of elements.
				for (auto& el: elem_json) elems.push_back(parse(el));
			} else {
				// An element grouping with possible pointer message pointer_messages.
				ASSUME_HAS(elem_json, "type");
				ASSUME_VAL(elem_json, "type", "grouping");

				if (elem_json.contains("pointer_messages")) {
					ASSUME_ARR(elem_json, "pointer_messages");
					pointer_messages = elem_json["pointer_messages"];
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

		std::shared_ptr<view_manager::Component> ConcatDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitConcatDElement(*this);
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

		DisplayPtr StartLineDElement::copy() const {
			return std::make_shared<StartLineDElement>(*this);
		}

		std::shared_ptr<view_manager::Component> StartLineDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitStartLineDElement(*this);
		}

		// ---------------- InteractDElement ---------------- //

		InteractDElement::InteractDElement(const json& elem_json) {
			// Both "entity" and "grouping" syntax elements can be
			// interactive.
			// Note that entity data is lost when creating an instance
			// of InteractDElement - that is why entities are recognised
			// before interacts upon parsing.
			ASSUME_HAS(elem_json, "type");
			CORE_ASSERT(
				elem_json["type"] == "entity" || elem_json["type"] == "grouping",
				"interactive element is neither entity nor a grouping"
			);

			ASSUME_HAS(elem_json, "content");
			content = parse(elem_json["content"]);
			ASSUME_HAS(elem_json, "alt_content");
			alt_content = parse(elem_json["alt_content"]);

			if (elem_json.contains("pointer_messages")) {
				ASSUME_ARR(elem_json, "pointer_messages");
				pointer_messages = elem_json["pointer_messages"];
			}
		}

		InteractDElement::InteractDElement(const InteractDElement& other):
			  DisplayElement(other),
			  content(other.content->copy()),
			  alt_content(other.alt_content->copy()) {}

		DisplayPtr InteractDElement::copy() const {
			return std::make_shared<InteractDElement>(*this);
		}

		std::shared_ptr<view_manager::Component> InteractDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitInteractDElement(*this);
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

		DisplayPtr LazyDElement::evaluated(ViewConstructor& vc) const {
			return vc.getLazyElement(id);
		}

		std::shared_ptr<view_manager::Component> LazyDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitLazyDElement(*this);
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

				if (elem_json.contains("pointer_messages")) {
					ASSUME_ARR(elem_json, "pointer_messages");
					pointer_messages = elem_json["pointer_messages"];
				}
			}
		}

		EntityDElement::EntityDElement(const EntityDElement& other):
			  DisplayElement(other),
			  refers_to(other.refers_to),
			  content(other.content->copy()) {}

		DisplayPtr EntityDElement::copy() const { return std::make_shared<EntityDElement>(*this); }

		std::set<InfoID> EntityDElement::getAssocInfos(ViewConstructor& vc) const {
			auto res        = assoc_infos;
			auto entity_res = vc.assocInfosFromEntity(refers_to);

			// Merge the two info pools.
			res.insert(entity_res.begin(), entity_res.end());
			return res;
		}

		std::shared_ptr<view_manager::Component> EntityDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitEntityDElement(*this);
		}

		// ---------------- CodeDElement ---------------- //

		CodeDElement::CodeDElement(const json& elem_json) {
			ASSUME_HAS(elem_json, "type");
			ASSUME_VAL(elem_json, "type", "code");
			ASSUME_HAS_STR(elem_json, "content");
			content = elem_json["content"];

			if (elem_json.contains("pointer_messages")) {
				ASSUME_ARR(elem_json, "pointer_messages");
				pointer_messages = elem_json["pointer_messages"];
			}
		}

		CodeDElement::CodeDElement(const CodeDElement& other):
			  DisplayElement(other),
			  content(other.content) {}

		DisplayPtr CodeDElement::copy() const { return std::make_shared<CodeDElement>(*this); }

		std::shared_ptr<view_manager::Component> CodeDElement::accept(ToComponentVisitor& visitor
		) const {
			return visitor.visitCodeDElement(*this);
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

		// ---------------- ToComponentVisitor ---------------- //

		void ToComponentVisitor::accumulateData(const DisplayElement& el) {
			// Add this element's pointer_messages to the accumulator.
			this->acc_data.pointer_messages.insert(el.pointer_messages.begin(), el.pointer_messages.end());
			// Add this element's associated infos to the accumulator
			// (may perform some fetching if it is an entity element and its
			// related entity has not yet been fetched).
			auto assoc_infos = el.getAssocInfos(this->vc);
			this->acc_data.assoc_infos.insert(assoc_infos.begin(), assoc_infos.end());
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitTextDElement(
			const TextDElement& el
		) {
			this->accumulateData(el);
			auto id = this->get_next_id();
			auto result
				= std::make_shared<TextComponent>(id, el.content, this->acc_data.getAssocInfos());
			return result;
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitCodeDElement(
			const CodeDElement& el
		) {
			this->accumulateData(el);
			auto id         = this->get_next_id();
			auto pointer_messages     = this->acc_data.getpointer_messages();
			auto tags_range = std::ranges::subrange(pointer_messages.begin(), pointer_messages.end())
			                | std::views::transform([this](const std::string& group) {
								  return this->group_to_id(group);
							  });
			auto tags = std::vector(tags_range.begin(), tags_range.end());
			return std::make_shared<CodeComponent>(
				id, el.content, std::move(tags), acc_data.getAssocInfos()
			);
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitConcatDElement(
			const ConcatDElement& el
		) {
			this->accumulateData(el);

			DisplayElement::AccData acc_data_to_this_point = this->acc_data;

			std::vector<std::shared_ptr<view_manager::Component>> children;
			for (const auto& son: el.elems) {
				this->acc_data = acc_data_to_this_point;
				children.emplace_back(son->accept(*this));
			}
			auto result = std::make_shared<ConcatComponent>(children);
			for (auto& child: children) child->setParent(result);
			return result;
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitStartLineDElement(
			const StartLineDElement& el
		) {
			return std::make_shared<StartLineComponent>(el.number);
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitInteractDElement(
			const InteractDElement& el
		) {
			this->accumulateData(el);

			DisplayElement::AccData acc_data_to_this_point = this->acc_data;

			auto id        = this->get_next_id();
			auto primary   = el.content->accept(*this);
			this->acc_data = acc_data_to_this_point;
			// TODO: Change to return a LazyComponent instead of evaluating `alternative`.
			auto alternative = el.alt_content->accept(*this);
			this->acc_data   = acc_data_to_this_point;

			auto result = std::make_shared<InteractiveComponent>(id, primary, alternative);
			result->primary->setParent(result);
			result->alternative->setParent(result);
			return result;
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitEntityDElement(
			const EntityDElement& el
		) {
			this->accumulateData(el);
			auto result = el.content->accept(*this);
			return result;
		}

		std::shared_ptr<view_manager::Component> ToComponentVisitor::visitLazyDElement(
			const LazyDElement& el
		) {
			this->accumulateData(el);
			auto result = el.evaluated(this->vc)->accept(*this);
			return result;
		}
	}  // namespace dia_file
}  // namespace dia_app
