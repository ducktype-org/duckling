#pragma once
#include "display_elements.hpp"
#include "utils.hpp"
#include "view_constructor.hpp"

#include <proto/view.pb.h>

#include <base/box.hpp>

namespace dia_app {
	namespace view_manager {
		/**
		 * @brief Unique identifier of a UI component inside a diagnostic.
		 */
		using component_id_t = u32;

		/**
		 * @brief Identifier of an entry that should be opened in the side panel
		 * when interacting with a component.
		 */
		using side_entry_id_t = InfoID;

		/**
		 * @brief Identifier of a highlight group assigned to code pieces.
		 */
		using hl_id_t = u32;
		/**
		 * @brief Priority used for ordering pointer messages. Lower is stronger.
		 */
		using priority_t = u32;

		/**
		 * @brief Source line number alias.
		 */
		using line_no_t = u32;
		/**
		 * @brief Source column number alias.
		 */
		using column_no_t = u32;


		/**
		 * @brief Optional line metadata (line number) to display before code.
		 */
		using line_metadata_t = base::Optional<uint>;

		/**
		 * @brief Optional payload rendered after a line (line suffix).
		 *
		 * Typically used to give a trailing, per-line element (e.g. pointer
		 * message continuation) alongside the normal line payloads.
		 */
		template<class T>
		using line_suffix_data_t = base::Optional<base::Box<T>>;

		/**
		 * @brief A pair of optional metadata and optional payload for a line.
		 */
		template<class T>
		using line_data_t = std::pair<line_metadata_t, base::Optional<base::Box<T>>>;

		/**
		 * @brief The result of rendering a component to a list of lines.
		 *
		 * First element is an optional suffix for the last line, followed by a
		 * vector of per-line metadata and payload pairs.
		 */
		template<class T>
		using component_get_view_data_t
			= std::pair<line_suffix_data_t<T>, std::vector<line_data_t<T>>>;


		struct TextComponent;
		struct CodeComponent;
		struct ConcatComponent;
		struct InteractiveComponent;
		struct StartLineComponent;

		/**
		 * @brief Visitor that serializes a component subtree into a view::HlComponent
		 * representation (i.e. with highlighting metadata).
		 *
		 * Produces a line-wise structure used by the UI to render code, line
		 * numbers and pointer messages with highlight groups.
		 */
		class GetHlViewVisitor {
		private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor& vc;

		public:
			// Construct the visitor bound to a diagnostic's ViewConstructor
			// for potential lazy fetches during serialization.
			GetHlViewVisitor(ViewConstructor& vc): vc(vc) {}

			component_get_view_data_t<::view::HlComponent> visitCodeComponent(
				const CodeComponent& component
			);
			component_get_view_data_t<::view::HlComponent> visitConcatComponent(
				const ConcatComponent& component
			);
			component_get_view_data_t<::view::HlComponent> visitInteractiveComponent(
				const InteractiveComponent& component
			);
			component_get_view_data_t<::view::HlComponent> visitStartLineComponent(
				const StartLineComponent& component
			);
		};

		/**
		 * @brief Visitor that serializes a component subtree into a
		 * view::NoHlComponent representation (plain text without highlight
		 * groups).
		 */
		class GetNoHlViewVisitor {
		private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor& vc;


		public:
			GetNoHlViewVisitor(ViewConstructor& vc): vc(vc) {}

			component_get_view_data_t<::view::NoHlComponent> visitTextComponent(
				const TextComponent& component
			);
			component_get_view_data_t<::view::NoHlComponent> visitCodeComponent(
				const CodeComponent& component
			);
			component_get_view_data_t<::view::NoHlComponent> visitConcatComponent(
				const ConcatComponent& component
			);
			component_get_view_data_t<::view::NoHlComponent> visitInteractiveComponent(
				const InteractiveComponent& component
			);
			component_get_view_data_t<::view::NoHlComponent> visitStartLineComponent(
				const StartLineComponent& component
			);
		};

		class SidePath;

		/**
		 * @brief Visitor that applies an interaction to a component subtree.
		 *
		 * Supported interactions include clicking a component, toggling
		 * interactive components, and rolling back an interactive click.
		 * This visitor also collects and opens side panel paths for
		 * associated infos.
		 */
		class InteractionVisitor {
		public:
			enum class InteractionType : uint8_t {
				Click,
				ClickInteractive,
				ClickInteractiveRollback
			};

		private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor&      vc;
			std::vector<SidePath> opened_side_paths;
			InteractionType       interaction;

		public:
			void visitTextComponent(TextComponent& component);
			void visitCodeComponent(CodeComponent& component);
			void visitConcatComponent(ConcatComponent& component);
			void visitInteractiveComponent(InteractiveComponent& component);
			void visitStartLineComponent(StartLineComponent& component);
		};

		/**
		 * @brief Abstract base of the view-manager component tree.
		 *
		 * Concrete subclasses represent text, code, concatenation, interactive
		 * branch and line breaks. Each component can be serialized either to a
		 * highlighted representation or to a plain-text one, and can react to
		 * interactions.
		 */
		class Component {
		public:
			base::Optional<std::weak_ptr<Component>> parent;

			Component() = default;
			Component(base::Optional<std::weak_ptr<Component>> parent);
			virtual ~Component();

			void setParent(const std::weak_ptr<Component>& p);

			/**
			 * @brief Serialize this subtree to a highlighted representation.
			 *
			 * Implemented by concrete subclasses by dispatching to the visitor.
			 */
			virtual component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor& visitor
			) const
				= 0;
			/**
			 * @brief Serialize this subtree to a non-highlighted representation.
			 */
			virtual component_get_view_data_t<::view::NoHlComponent> accept(
				GetNoHlViewVisitor& visitor
			) const
				= 0;

			/**
			 * @brief Apply an interaction to this subtree.
			 */
			virtual void accept(InteractionVisitor& visitor) = 0;

			/**
			 * @brief Reset the internal state of this component.
			 *
			 * Used to revert interactions and return to the initial display.
			 */
			virtual void reset() = 0;
		};

		using component_context_t
			= std::pair<std::shared_ptr<Component>, std::shared_ptr<ViewConstructor>>;

		/**
		 * @brief Leaf component holding simple text.
		 *
		 * Can have associated side entries, but does not support highlighting
		 * (attempting to serialize as highlighted is prohibited).
		 */
		class TextComponent: public Component {
		private:
			component_id_t               id;
			std::string                  content;
			std::vector<side_entry_id_t> assoc_side_entries;

			friend class GetNoHlViewVisitor;
			friend class InteractionVisitor;

		public:
			TextComponent(
				component_id_t               id,
				std::string                  content,
				std::vector<side_entry_id_t> assoc_side_entries
			);

			component_get_view_data_t<::view::HlComponent>   accept(GetHlViewVisitor& visitor
			  ) const override;  // Not permitted.
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor& visitor
			) const override;
			void accept(InteractionVisitor& visitor) override;

			void reset() override;
		};

		/**
		 * @brief Leaf component holding a piece of code with highlight groups.
		 *
		 * Each code piece can participate in multiple highlight groups that
		 * tie into pointer messages within a code section.
		 */
		class CodeComponent: public Component {
		private:
			component_id_t               id;
			std::string                  content;
			std::vector<hl_id_t>         tags;
			std::vector<side_entry_id_t> assoc_side_entries;

			friend class GetHlViewVisitor;
			friend class GetNoHlViewVisitor;
			friend class InteractionVisitor;

		public:
			CodeComponent(
				component_id_t               id,
				std::string                  content,
				std::vector<hl_id_t>         tags,
				std::vector<side_entry_id_t> assoc_side_infos
			);

			component_get_view_data_t<::view::HlComponent>   accept(GetHlViewVisitor& visitor
			  ) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor& visitor
			) const override;
			void accept(InteractionVisitor& visitor) override;

			void reset() override;
		};

		/**
		 * @brief Node component concatenating multiple child components in order.
		 */
		class ConcatComponent: public Component {
		private:

		public:
			std::vector<std::shared_ptr<Component>> components;
			ConcatComponent(std::vector<std::shared_ptr<Component>> components);

			component_get_view_data_t<::view::HlComponent>   accept(GetHlViewVisitor& visitor
			  ) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor& visitor
			) const override;
			void accept(InteractionVisitor& visitor) override;

			void reset() override;
		};

		/**
		 * @brief Node component that toggles between primary and alternative
		 * content upon interaction.
		 */
		class InteractiveComponent: public Component {
		public:
			enum class Status : bool { Primary, Alternative };

		private:
			component_id_t id;
			Status         status = Status::Primary;

			friend class GetHlViewVisitor;
			friend class GetNoHlViewVisitor;
			friend class InteractionVisitor;

		public:
			std::shared_ptr<Component> primary, alternative;
			InteractiveComponent(
				component_id_t                    id,
				const std::shared_ptr<Component>& primary,
				const std::shared_ptr<Component>& alternative
			);

			component_get_view_data_t<::view::HlComponent>   accept(GetHlViewVisitor& visitor
			  ) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor& visitor
			) const override;
			void accept(InteractionVisitor& visitor) override;

			void reset() override;
		};

		/**
		 * @brief Leaf component that inserts a line break and optional line number.
		 *
		 * Line numbers are honored when placed within a code section.
		 */
		class StartLineComponent: public Component {
		private:
			base::Optional<uint> number;

			friend class GetHlViewVisitor;
			friend class GetNoHlViewVisitor;
			friend class InteractionVisitor;

		public:
			StartLineComponent(base::Optional<uint> number);

			component_get_view_data_t<::view::HlComponent>   accept(GetHlViewVisitor& visitor
			  ) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor& visitor
			) const override;
			void accept(InteractionVisitor& visitor) override;

			void reset() override;
		};
	}  // namespace view_manager
}  // namespace dia_app
