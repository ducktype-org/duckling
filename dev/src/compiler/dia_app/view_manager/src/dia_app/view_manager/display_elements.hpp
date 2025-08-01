#pragma once
#include <utility>
#include <base/visitor.hpp>
#include <base/visitor.hpp>

#include "utils.hpp"
#include "view_constructor.hpp"

namespace dia_app {
	namespace view_manager {
		class Component;
		class CreationContext;
	}

	namespace dia_file {

		// Element which can be displayed as a part of text.
		struct DisplayElement;
		using DisplayPtr = std::shared_ptr<DisplayElement>;

		DisplayPtr parse(const json& msg);

		struct TextDElement;
		struct CodeDElement;
		struct ConcatDElement;
		struct StartLineDElement;
		struct InteractDElement;
		struct EntityDElement;
		struct LazyDElement;

		MAKE_VISITOR(DisplayElement,
			TextDElement,
			CodeDElement,
			ConcatDElement,
			StartLineDElement,
			InteractDElement,
			EntityDElement,
			LazyDElement
		);

		struct DisplayElement {
			std::set<InfoID>  assoc_infos;
			std::set<std::string> groups;

			DisplayElement() = default;

			DisplayElement(const DisplayElement& other):
				  assoc_infos(other.assoc_infos),
				  groups(other.groups) {}

			virtual ~DisplayElement() = default;

			// Make a deep copy of this element.
			virtual DisplayPtr copy() const = 0;

			// Get all associated infos on this particular element.
			// Note: this does *not* get all associated infos from
			//       the entire subtree.
			virtual std::set<InfoID> getAssocInfos(ViewConstructor &vc) const { return assoc_infos; }

			// Check whether this particular element has
			// any associated infos. May fetch some entities.
			bool hasAssocInfos(ViewConstructor &vc) const { return getAssocInfos(vc).size() > 0; }

			// Accumulation data for to-component conversion.
			struct AccData {
				std::set<InfoID>  assoc_infos;
				std::set<std::string> groups;

				std::vector<InfoID> getAssocInfos() const {
					return std::vector<InfoID>(assoc_infos.begin(), assoc_infos.end());
				}

				std::vector<std::string> getGroups() const {
					return std::vector<std::string>(groups.begin(), groups.end());
				}
			};

			// Append this element's data (assoc_infos and groups)
			// to the provided accumulators.
			void accumulateData(std::shared_ptr<view_manager::CreationContext> creation_context, AccData& acc) const;

			// Convert this element to a view manager component
			// and cache the result.
			std::shared_ptr<view_manager::Component> toComponent(
				std::shared_ptr<view_manager::CreationContext> creation_context
			) {
				return toComponentImpl(creation_context, AccData());
			}

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) = 0;

			virtual void accept(DisplayElementVisitor &visitor) = 0;
			virtual void accept(DisplayElementVisitor &visitor) = 0;
		};

		struct TextDElement: public DisplayElement {
			std::string content;

			TextDElement(const json& elem_json);
			TextDElement(const TextDElement& other);

			DisplayPtr         copy() const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitTextDElement(*this);
			}
		};

		struct ConcatDElement: public DisplayElement {
			std::vector<DisplayPtr> elems;

			ConcatDElement(const json& elem_json);
			ConcatDElement(const ConcatDElement& other);
			// Auxiliary constructor used in template_elements.hpp.
			// Note: does not deep copy the input pointers.
			ConcatDElement(const std::vector<DisplayPtr>& elems);

			DisplayPtr         copy() const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitConcatDElement(*this);
			}
		};

		struct StartLineDElement: public DisplayElement {
			// Line number.
			base::Optional<u32> number;

			StartLineDElement(const json& elem_json);
			StartLineDElement(const StartLineDElement& other);

			DisplayPtr         copy() const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitStartLineDElement(*this);
			}
		};

		struct InteractDElement: public DisplayElement {
			DisplayPtr content;
			DisplayPtr alt_content;

			InteractDElement(const json& elem_json);
			InteractDElement(const InteractDElement& other);

			DisplayPtr         copy() const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitInteractDElement(*this);
			}
		};

		struct LazyDElement: public DisplayElement {
			LazyDisplayID id;

			LazyDElement(const json& elem_json);
			LazyDElement(const LazyDElement& other);

			DisplayPtr         copy() const;

			DisplayPtr evaluated(ViewConstructor &vc) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitLazyDElement(*this);
			}
		};

		struct EntityDElement: public DisplayElement {
			std::string refers_to;
			DisplayPtr         content;

			EntityDElement(const json& elem_json);
			EntityDElement(const EntityDElement& other);

			DisplayPtr                  copy() const;
			std::set<InfoID> getAssocInfos(ViewConstructor &vc) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitEntityDElement(*this);
			}
		};

		struct CodeDElement: public DisplayElement {
			std::string content;

			CodeDElement(const json& elem_json);
			CodeDElement(const CodeDElement& other);

			DisplayPtr copy() const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor &visitor) {
				visitor.visitCodeDElement(*this);
			}
		};

		// Evaluate the pure text content of an element.
		// Only evaluates the default content (alt_content is ignored).
		// Used e.g. for "case of" matching in message templates.
		struct ToTextVisitor : public DisplayElementVisitor {
			ViewConstructor &vc;
			std::string builder;

			ToTextVisitor(ViewConstructor &vc) : vc(vc) {}

			virtual void visitTextDElement(const TextDElement &el) {
				builder += el.content;
			}
			virtual void visitCodeDElement(const CodeDElement &el) {
				builder += el.content;
			}
			virtual void visitConcatDElement(const ConcatDElement &el) {
				for (auto &child : el.elems) {
					child->accept(*this);
				}
			}
			virtual void visitStartLineDElement(const StartLineDElement &el) {
				builder += '\n';
			}
			virtual void visitInteractDElement(const InteractDElement &el) {
				// Only consider the primary content of this element.
				// According to the default serialization semantics,
				// alternative content is to be ignored.
				el.content->accept(*this);
			}
			virtual void visitEntityDElement(const EntityDElement &el) {
				el.content->accept(*this);
			}
			virtual void visitLazyDElement(const LazyDElement &el) {
				// Serialize the evaluated subtree of this element.
				el.evaluated(vc)->accept(*this);
			}
		};

		// @TODO implement toComponent in this visitor pattern.
		// This will enable great decoupling between the View Constructor
		// and View Manager source code and facilitate further development
		// of the laziness mechanism.
		struct ToComponentVisitor : public DisplayElementVisitor {
			ViewConstructor &vc;
			// @TODO creation_context contains some useless fields, including a separate view constructor.
			// this can all be extracted to fields of this class.
			std::shared_ptr<CreationContext> creation_context;

			// Data accumulated during the search.
			DisplayElement::AccData accData;

			std::shared_ptr<Component> result;
		};

	}  // namespace dia_file
}  // namespace dia_app
