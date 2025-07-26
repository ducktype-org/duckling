#pragma once
#include <utility>

#include "utils.hpp"

namespace dia_app {
	namespace view_manager {
		class Component;
		class CreationContext;
	}

	namespace dia_file {

		// Element which can be displayed as a part of text.
		struct DisplayElement;
		using Ptr = std::shared_ptr<DisplayElement>;

		Ptr parse(const json& msg);

		struct TextElement;
		struct CodeElement;
		struct ConcatElement;
		struct StartLineElement;
		struct InteractElement;
		struct EntityElement;
		struct LazyElement;

		struct DisplayElementVisitor {
			virtual void visitText(TextElement *el) = 0;
			virtual void visitCode(CodeElement *el) = 0;
			virtual void visitConcat(ConcatElement *el) = 0;
			virtual void visitStartLine(StartLineElement *el) = 0;
			virtual void visitInteract(InteractElement *el) = 0;
			virtual void visitEntity(EntityElement *el) = 0;
			virtual void visitLazy(LazyElement *el) = 0;
		};

		struct DisplayElement {
			std::set<InfoHandle>  assoc_infos;
			std::set<std::string> groups;

			// Cached component generated from this display element.
			std::shared_ptr<view_manager::Component> generated_component;

			DisplayElement() = default;

			DisplayElement(const DisplayElement& other):
				  assoc_infos(other.assoc_infos),
				  groups(other.groups) {}

			virtual ~DisplayElement() = default;

			// Make a deep copy of this element.
			virtual Ptr copy() const = 0;

			// Get all associated infos on this particular element.
			// Note: this does *not* get all associated infos from
			//       the entire subtree.
			virtual std::set<InfoHandle> getAssocInfos(DataHandle dh) const { return assoc_infos; }

			// Check whether this particular element has
			// any associated infos. May fetch some entities.
			bool hasAssocInfos(DataHandle dh) const { return getAssocInfos(dh).size() > 0; }

			// Evaluate the pure text content of this element.
			// Only evaluates the default content (alt_content is ignored).
			// Used e.g. for "case of" matching in message templates.
			virtual std::string toText(DataHandle dh) const = 0;

			// Serialize this element (DEBUG ONLY).
			virtual json show(DataHandle dh) const = 0;

			// Accumulation data for to-component conversion.
			struct AccData {
				std::set<InfoHandle>  assoc_infos;
				std::set<std::string> groups;

				std::vector<InfoHandle> getAssocInfos() const {
					return std::vector<InfoHandle>(assoc_infos.begin(), assoc_infos.end());
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

			virtual void accept(DisplayElementVisitor *visitor) = 0;
		};

		struct TextElement: public DisplayElement {
			std::string content;

			TextElement(const json& elem_json);
			TextElement(const TextElement& other);

			Ptr         copy() const;
			std::string toText(DataHandle _) const;

			json show(DataHandle _) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitText(this);
			}
		};

		struct ConcatElement: public DisplayElement {
			std::vector<Ptr> elems;

			ConcatElement(const json& elem_json);
			ConcatElement(const ConcatElement& other);
			// Auxiliary constructor used in template_elements.hpp.
			// Note: does not deep copy the input pointers.
			ConcatElement(const std::vector<Ptr>& elems);

			Ptr         copy() const;
			std::string toText(DataHandle dh) const;

			json show(DataHandle dh) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitConcat(this);
			}
		};

		struct StartLineElement: public DisplayElement {
			// Line number.
			std::optional<uint> number;

			StartLineElement(const json& elem_json);
			StartLineElement(const StartLineElement& other);

			Ptr         copy() const;
			std::string toText(DataHandle dh) const;

			json show(DataHandle dh) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitStartLine(this);
			}
		};

		struct InteractElement: public DisplayElement {
			Ptr content;
			Ptr alt_content;

			InteractElement(const json& elem_json);
			InteractElement(const InteractElement& other);

			Ptr         copy() const;
			std::string toText(DataHandle dh) const;

			json show(DataHandle dh) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitInteract(this);
			}
		};

		struct LazyElement: public DisplayElement {
			ResourceHandle handle;

			LazyElement(const json& elem_json);
			LazyElement(const LazyElement& other);

			Ptr         copy() const;
			std::string toText(DataHandle dh) const;

			Ptr evaluated(DataHandle dh) const;

			json show(DataHandle dh) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitLazy(this);
			}
		};

		struct EntityElement: public DisplayElement {
			std::string refers_to;
			Ptr         content;

			EntityElement(const json& elem_json);
			EntityElement(const EntityElement& other);

			Ptr                  copy() const;
			std::set<InfoHandle> getAssocInfos(DataHandle dh) const;

			std::string toText(DataHandle dh) const;

			json show(DataHandle dh) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitEntity(this);
			}
		};

		struct CodeElement: public DisplayElement {
			std::string content;

			CodeElement(const json& elem_json);
			CodeElement(const CodeElement& other);

			Ptr copy() const;

			std::string toText(DataHandle dh) const;

			json show(DataHandle dh) const;

			virtual std::shared_ptr<view_manager::Component> toComponentImpl(
				std::shared_ptr<view_manager::CreationContext> creation_context, AccData acc_data
			) override;

			virtual void accept(DisplayElementVisitor *visitor) {
				visitor->visitCode(this);
			}
		};

	}  // namespace dia_file
}  // namespace dia_app
