#pragma once
#include "display_elements.hpp"
#include "utils.hpp"
#include "view_constructor.hpp"

#include <proto/view.pb.h>

#include <base/box.hpp>

namespace dia_app {
	namespace view_manager {
		using component_id_t = u32;

		using side_entry_id_t = InfoID;

		using hl_id_t    = u32;
		using priority_t = u32;

		using line_no_t   = u32;
		using column_no_t = u32;


		using line_metadata_t = base::Optional<uint>;

		template<class T>
		using line_suffix_data_t = base::Optional<base::Box<T>>;

		template<class T>
		using line_data_t = std::pair<line_metadata_t, base::Optional<base::Box<T>>>;

		template<class T>
		using component_get_view_data_t
			= std::pair<line_suffix_data_t<T>, std::vector<line_data_t<T>>>;


		struct TextComponent;
		struct CodeComponent;
		struct ConcatComponent;
		struct InteractiveComponent;
		struct StartLineComponent;

		class GetHlViewVisitor {
		private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor& vc;

		public:
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

		struct InteractionContext {
			base::Optional<std::function<void(std::vector<SidePath>)>> after_open;
		};

		class Component {
		public:
			base::Optional<std::weak_ptr<Component>> parent;

			Component() = default;
			Component(base::Optional<std::weak_ptr<Component>> parent);
			virtual ~Component();

			void setParent(const std::weak_ptr<Component>& p);

			virtual component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor& visitor
			) const
				= 0;
			virtual component_get_view_data_t<::view::NoHlComponent> accept(
				GetNoHlViewVisitor& visitor
			) const
				= 0;
			virtual void accept(InteractionVisitor& visitor) = 0;

			virtual void reset() = 0;
		};

		using component_context_t
			= std::pair<std::shared_ptr<Component>, std::shared_ptr<ViewConstructor>>;

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
