#pragma once
#include "utils.hpp"

#include <base/box.hpp>
#include <base/visitor.hpp>

namespace dia_app {
	struct ViewConstructor;

	namespace view_manager {
		class Component;
		using hl_id_t = u32;
	}

	namespace dia_file {

		// Forward declarations of display elements and a display visitor.

		// Element which can be displayed as a part of text.
		struct DisplayElement;
		using DisplayPtr = std::shared_ptr<DisplayElement>;

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

		class ToComponentVisitor;

		/**
		 * @brief Parse the `json` representation of a display element.
		 *
		 * @param msg The `json` representation
		 * @return DisplayPtr
		 */
		DisplayPtr parse(const json& msg);

		/**
		 * @brief A base class display element.
		 *
		 * A display element represents a message fragment inside a diagnostic
		 * file or inside the view constructor's internal representation.
		 * It can define the text or code to be displayed, additional metadata,
		 * interactive content, or represent a lazily fetched subtree of display
		 * elements.
		 *
		 */
		struct DisplayElement {
			/**
			 * @brief Every display element has a set of infos associated
			 * with it.
			 *
			 * Associated infos are the infos which are supposed to be displayed
			 * to the user upon a certain interaction with this element, e.g. by
			 * showing a note with a declaration of a variable that a particular
			 * display element refers to.
			 *
			 */
			std::set<InfoID> assoc_infos;
			/**
			 * @brief Every display element has a set of groups it belongs to.
			 *
			 * Display element groups are only used inside the info code
			 * section's contents.
			 *
			 * They refer to specific pointer messages in that code section.
			 * An element containing a specific group means that this
			 * element is to be underlined and pointed to by the corresponding
			 * pointer message.
			 *
			 */
			std::set<std::string> groups;

			DisplayElement() = default;

			DisplayElement(const DisplayElement& other);

			virtual ~DisplayElement() = default;

			// Make a deep copy of this element.
			virtual DisplayPtr copy() const = 0;

			// Get all associated infos on this particular element.
			// May fetch some entities.
			virtual std::set<InfoID> getAssocInfos(ViewConstructor& vc) const {
				return assoc_infos;
			}

			// Check whether this particular element has any associated infos.
			// May fetch some entities.
			bool hasAssocInfos(ViewConstructor& vc) const { return getAssocInfos(vc).size() > 0; }

			// Accumulation data for to-component conversion.
			struct AccData {
				std::set<InfoID>      assoc_infos;
				std::set<std::string> groups;

				std::vector<InfoID> getAssocInfos() const {
					return std::vector<InfoID>(assoc_infos.begin(), assoc_infos.end());
				}

				std::vector<std::string> getGroups() const {
					return std::vector<std::string>(groups.begin(), groups.end());
				}
			};

			virtual void accept(DisplayElementVisitor& visitor) = 0;

			virtual std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const
				= 0;
		};

		/**
		 * @brief A display element representing simple text.
		 *
		 */
		struct TextDElement: public DisplayElement {
			std::string content;

			TextDElement(const json& elem_json);
			TextDElement(const TextDElement& other);

			DisplayPtr copy() const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitTextDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A display element representing a concatenation of other
		 * display elements.
		 *
		 */
		struct ConcatDElement: public DisplayElement {
			std::vector<DisplayPtr> elems;

			ConcatDElement(const json& elem_json);
			ConcatDElement(const ConcatDElement& other);
			// Auxiliary constructor used in template_elements.hpp.
			// Note: does not deep copy the input pointers.
			ConcatDElement(const std::vector<DisplayPtr>& elems);

			DisplayPtr copy() const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitConcatDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A display element representing a start of a new line.
		 * It may optionally contain the new line's number in the source file
		 * (which is considered only when the element resides within the info's
		 * code section).
		 *
		 */
		struct StartLineDElement: public DisplayElement {
			// Line number.
			base::Optional<u32> number;

			StartLineDElement(const json& elem_json);
			StartLineDElement(const StartLineDElement& other);

			DisplayPtr copy() const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitStartLineDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A display element representing two pieces of content
		 * alternating upon user interaction.
		 *
		 */
		struct InteractDElement: public DisplayElement {
			DisplayPtr content;
			DisplayPtr alt_content;

			InteractDElement(const json& elem_json);
			InteractDElement(const InteractDElement& other);

			DisplayPtr copy() const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitInteractDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A display element representing an initially unfetched element
		 * subtree.
		 *
		 * Note that since display elements are immutable, lazy elements are
		 * not removed from the element tree upon fetching their contents.
		 * Instead, the fetched contents reside inside the corresponding
		 * view constructor and can be queried by this lazy element.
		 *
		 * Note: if the lazy element has any `assoc_infos` or `groups` data,
		 * it is ignored upon fetching its the `evaluated` content.
		 *
		 */
		struct LazyDElement: public DisplayElement {
			LazyDisplayID id;

			LazyDElement(const json& elem_json);
			LazyDElement(const LazyDElement& other);

			DisplayPtr copy() const;

			DisplayPtr evaluated(ViewConstructor& vc) const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitLazyDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A display element representing content related to a specific
		 * entity in the info group.
		 *
		 * Calling `getAssocInfos` queries the related entity's contents
		 * and may add more associated infos to the result than the ones
		 * present in the `assoc_infos` field of the `DisplayElement` base
		 * class.
		 *
		 */
		struct EntityDElement: public DisplayElement {
			EntityID   refers_to;
			DisplayPtr content;

			EntityDElement(const json& elem_json);
			EntityDElement(const EntityDElement& other);

			DisplayPtr       copy() const;
			std::set<InfoID> getAssocInfos(ViewConstructor& vc) const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitEntityDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A display element representing a piece of code.
		 *
		 */
		struct CodeDElement: public DisplayElement {
			std::string content;

			CodeDElement(const json& elem_json);
			CodeDElement(const CodeDElement& other);

			DisplayPtr copy() const;

			virtual void accept(DisplayElementVisitor& visitor) {
				visitor.visitCodeDElement(*this);
			}

			std::shared_ptr<view_manager::Component> accept(ToComponentVisitor& visitor
			) const override;
		};

		/**
		 * @brief A visitor which evaluates the pure text content of a display
		 * element.
		 *
		 * Only the default content of the element tree is added to the result
		 * (the alternative content of `InteractiveDElement` is ignored).
		 *
		 * Used e.g. in the `case ... of ...` template element from info
		 * templates.
		 */
		struct ToTextVisitor: public DisplayElementVisitor {
			ViewConstructor& vc;
			std::string      builder;

			ToTextVisitor(ViewConstructor& vc): vc(vc) {}

			virtual void visitTextDElement(const TextDElement& el) { builder += el.content; }

			virtual void visitCodeDElement(const CodeDElement& el) { builder += el.content; }

			virtual void visitConcatDElement(const ConcatDElement& el) {
				for (auto& child: el.elems) child->accept(*this);
			}

			virtual void visitStartLineDElement(const StartLineDElement& el) { builder += '\n'; }

			virtual void visitInteractDElement(const InteractDElement& el) {
				// Only consider the primary content of this element.
				// According to the default serialization semantics,
				// alternative content is to be ignored.
				el.content->accept(*this);
			}

			virtual void visitEntityDElement(const EntityDElement& el) {
				el.content->accept(*this);
			}

			virtual void visitLazyDElement(const LazyDElement& el) {
				// Serialize the evaluated subtree of this element.
				el.evaluated(vc)->accept(*this);
			}
		};

		// @TODO implement toComponent in this visitor pattern.
		// This will enable great decoupling between the View Constructor
		// and View Manager source code and facilitate further development
		// of the laziness mechanism.
		class ToComponentVisitor {
		private:
			ViewConstructor& vc;
			// Data accumulated during the search.
			DisplayElement::AccData                           acc_data;
			std::function<u32()>                              get_next_id;
			std::function<view_manager::hl_id_t(std::string)> group_to_id;

			void accumulateData(const DisplayElement& el);

		public:
			std::shared_ptr<view_manager::Component> visitTextDElement(const TextDElement& el);
			std::shared_ptr<view_manager::Component> visitCodeDElement(const CodeDElement& el);
			std::shared_ptr<view_manager::Component> visitConcatDElement(const ConcatDElement& el);
			std::shared_ptr<view_manager::Component> visitStartLineDElement(
				const StartLineDElement& el
			);
			std::shared_ptr<view_manager::Component> visitInteractDElement(const InteractDElement& el
			);
			std::shared_ptr<view_manager::Component> visitEntityDElement(const EntityDElement& el);
			std::shared_ptr<view_manager::Component> visitLazyDElement(const LazyDElement& el);
		};
	}  // namespace dia_file
}  // namespace dia_app
