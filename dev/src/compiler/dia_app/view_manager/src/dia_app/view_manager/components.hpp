#pragma once
#include "utils.hpp"
#include "view_constructor.hpp"

#include <proto/view.pb.h>

namespace dia_app {
	namespace view_manager {
		using component_id_t = u32;

		static_assert(
			std::is_same_v<InfoID, u32>, "Ensure that side_entry match the protocol!"
		);
		using side_entry_id_t = InfoID;

		using hl_id_t    = u32;
		using priority_t = u32;

		using line_no_t   = u32;
		using column_no_t = u32;


		using line_metadata_t = base::Optional<uint>;

		template<class T>
		using line_suffix_data_t = base::Optional<std::unique_ptr<T>>;

		template<class T>
		using line_data_t = std::pair<line_metadata_t, base::Optional<std::unique_ptr<T>>>;

		template<class T>
		using component_get_view_data_t
			= std::pair<line_suffix_data_t<T>, std::vector<line_data_t<T>>>;




		struct TextComponent;
		struct CodeComponent;
		struct ConcatComponent;
		struct InteractiveComponent;
		struct StartLineComponent;
        
		// struct CreationContext {
		// 	std::shared_ptr<id_to_interactive_component_mapping_t> id_to_component;
		// 	std::shared_ptr<id_to_view_constructor_mapping_t>      id_to_view_constructor;
		// 	std::unique_ptr<std::map<std::string, hl_id_t>>        hl_name_to_id;
		// 	base::Optional<DataHandle>                              data_handle;
		// 	std::weak_ptr<ViewConstructor>                         view_constructor;

        //     CreationContext(
        //         std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component,
        //         std::shared_ptr<id_to_view_constructor_mapping_t>      id_to_view_constructor,
        //         std::unique_ptr<std::map<std::string, hl_id_t>>        hl_name_to_id,
        //         base::Optional<DataHandle>                              data_handle,
        //         std::weak_ptr<ViewConstructor>                         view_constructor
        //     );
		// };

		class GetHlViewVisitor {
			private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor &vc;
			base::HashMap<std::string, hl_id_t> hl_name_to_id;

			public:
			component_get_view_data_t<::view::HlComponent> visitTextComponent(const TextComponent &component);
			component_get_view_data_t<::view::HlComponent> visitCodeComponent(const CodeComponent &component);
			component_get_view_data_t<::view::HlComponent> visitConcatComponent(const ConcatComponent &component);
			component_get_view_data_t<::view::HlComponent> visitInteractiveComponent(const InteractiveComponent &component);
			component_get_view_data_t<::view::HlComponent> visitStartLineComponent(const StartLineComponent &component);
		};

		class GetNoHlViewVisitor {
			private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor &vc;
			base::HashMap<std::string, hl_id_t> hl_name_to_id;


			public:
			component_get_view_data_t<::view::NoHlComponent> visitTextComponent(const TextComponent &component);
			component_get_view_data_t<::view::NoHlComponent> visitCodeComponent(const CodeComponent &component);
			component_get_view_data_t<::view::NoHlComponent> visitConcatComponent(const ConcatComponent &component);
			component_get_view_data_t<::view::NoHlComponent> visitInteractiveComponent(const InteractiveComponent &component);
			component_get_view_data_t<::view::NoHlComponent> visitStartLineComponent(const StartLineComponent &component);
		};

		class SidePath;

		class InteractionVisitor {
			public:
			enum class InteractionType : uint8_t { Click, ClickInteractive, ClickInteractiveRollback };

			private:
			// `ViewConstructor` responsible for creation of the `Diagnostic` inside which we visit.
			ViewConstructor &vc;
			std::vector<SidePath> opened_side_paths;
			InteractionType interaction;

			public:
			void visitTextComponent(const TextComponent &component);
			void visitCodeComponent(const CodeComponent &component);
			void visitConcatComponent(const ConcatComponent &component);
			void visitInteractiveComponent(const InteractiveComponent &component);
			void visitStartLineComponent(const StartLineComponent &component);
		};

		struct InteractionContext {
			base::Optional<std::function<void(std::vector<SidePath>)>> after_open;
            std::shared_ptr<CreationContext> creation_context;
		};

		// inline std::string print(InteractionType it) {
		// 	switch (it) {
		// 	case dia_app::view_manager::InteractionType::Click:
		// 		return "Click";
		// 	case dia_app::view_manager::InteractionType::ClickInteractive:
		// 		return "ClickInteractive";
		// 	case dia_app::view_manager::InteractionType::ClickInteractiveRollback:
		// 		return "ClickInteractiveRollback";
		// 	}
		// 	assert(false);
		// }

		class Component {
			public:
			std::weak_ptr<Component>       parent;

			Component(std::weak_ptr<Component>       parent);
			virtual ~Component();

			virtual component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor &visitor) const = 0;
			virtual component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor &visitor) const = 0;
			virtual void accept(InteractionVisitor &visitor) = 0;

			virtual void reset() = 0;
		};

		using component_context_T = std::pair<std::shared_ptr<Component>, std::shared_ptr<ViewConstructor>>;

		class TextComponent: public Component {
		private:
			component_id_t id;
			std::string                  content;
			std::vector<side_entry_id_t> assoc_side_entries;

		public:
			TextComponent(
				component_id_t id,
				std::weak_ptr<ViewConstructor> view_constructor,
				std::string                    content,
				std::vector<side_entry_id_t>   assoc_side_entries
			);

			component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor &visitor) const override; // Not permitted.
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor &visitor) const override;
			void accept(InteractionVisitor &visitor) override;

			void reset() override;
		};

		class CodeComponent: public Component {
		private:
			component_id_t id;
			std::string                  content;
			std::vector<hl_id_t>         tags;
			std::vector<side_entry_id_t> assoc_side_entries;

		public:
			CodeComponent(
				component_id_t id,
				std::weak_ptr<ViewConstructor> view_constructor,
				std::string                    content,
				std::vector<hl_id_t>           tags,
				std::vector<side_entry_id_t>   assoc_side_infos
			);

			component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor &visitor) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor &visitor) const override;
			void accept(InteractionVisitor &visitor) override;

			void reset() override;
		};

		class ConcatComponent: public Component {
		private:

		public:
			std::vector<std::shared_ptr<Component>> components;
			ConcatComponent(
				std::weak_ptr<ViewConstructor>          view_constructor,
				std::vector<std::shared_ptr<Component>> components
			);

			component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor &visitor) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor &visitor) const override;
			void accept(InteractionVisitor &visitor) override;

			void reset() override;
		};

		class InteractiveComponent: public Component {
		public:
			enum class Status : bool { Primary, Alternative };

		private:
			component_id_t                                         id;
			Status                                                 status = Status::Primary;
			std::shared_ptr<Component>                             primary, alternative;

		public:
			InteractiveComponent(
				component_id_t                                         id,
				const std::shared_ptr<Component>&                      primary,
				const std::shared_ptr<Component>&                      alternative
			);

			component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor &visitor) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor &visitor) const override;
			void accept(InteractionVisitor &visitor) override;

			void reset() override;
		};

		class StartLineComponent: public Component {
		private:
			base::Optional<uint> number;

		public:
			StartLineComponent(
				base::Optional<uint>            number
			);

			component_get_view_data_t<::view::HlComponent> accept(GetHlViewVisitor &visitor) const override;
			component_get_view_data_t<::view::NoHlComponent> accept(GetNoHlViewVisitor &visitor) const override;
			void accept(InteractionVisitor &visitor) override;

			void reset() override;
		};

		// template<class T>
		// inline std::vector<std::unique_ptr<T>> filterEmptyLines(
		// 	component_get_view_data_t<T> component_view_data
		// ) {
		// 	auto& [suffix, mid] = component_view_data;
		// 	std::vector<std::unique_ptr<T>> components;
		// 	if (suffix.has_value()) components.emplace_back(std::move(suffix.value()));
		// 	std::vector<std::unique_ptr<T>> mid_components;
		// 	for (auto& [_, component]: mid)
		// 		if (component.has_value())
		// 			mid_components.emplace_back(std::move(component.value()));
		// 	components.insert(
		// 		components.end(),
		// 		std::make_move_iterator(mid_components.begin()),
		// 		std::make_move_iterator(mid_components.end())
		// 	);
		// 	return components;
		// }

		// std::unique_ptr<::view::NoHlComponent> concatNoHlLines(
		// 	std::vector<std::unique_ptr<::view::NoHlComponent>> lines
		// );
	}  // namespace view_manager
}  // namespace dia_app
