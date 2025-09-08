#pragma once
#include "components.hpp"
#include "view_constructor.hpp"

#include <base/box.hpp>

namespace dia_app {
	namespace view_manager {
		class CodeMetadata {
		private:
			std::string filename;
			line_no_t   line;
			column_no_t column;

		public:
			CodeMetadata(std::string filename, line_no_t line, column_no_t column);

			static base::Box<CodeMetadata> createFromLocation(
				const dia_file::CodeData::Location& location
			);

			base::Box<::view::CodeMetadata> getView() const;
		};

		class HlMessage {
		private:
			hl_id_t                    tag;
			priority_t                 priority;
			InfoType                   type;
			std::shared_ptr<Component> content;

		public:
			HlMessage(
				hl_id_t tag, priority_t priority, InfoType type, std::shared_ptr<Component> content
			);

			base::Box<::view::HlMessage> getView() const;
		};

		class Section {
		private:

		public:
			virtual ~Section();

			virtual base::Box<::view::Section> getView() const;
		};

		class TextSection: public Section {
		private:
			std::shared_ptr<Component> root;

		public:
			TextSection(std::shared_ptr<Component> root);

			base::Box<::view::Section> getView() const override;
		};

		class CodeSection: public Section {
		private:
			CodeMetadata               code_metadata;
			std::shared_ptr<Component> root;
			std::vector<HlMessage>     hl_messages;

		public:
			CodeSection(
				CodeMetadata               code_metadata,
				std::shared_ptr<Component> root,
				std::vector<HlMessage>     hl_messages
			);

			static base::Box<CodeSection> createFromInfo(
				const message_template::Info&                       info,
				std::shared_ptr<ViewConstructor>&                   view_constructor,
				base::HashMap<component_id_t, component_context_t>& id_to_component_context
			);

			base::Box<::view::Section> getView() const override;
		};

		using error_code_t = uint32_t;

		class Metadata {
		private:
			InfoType     type;
			error_code_t code;

		public:
			Metadata(InfoType type, error_code_t code);

			static Metadata createFromInfo(const message_template::Info& info);

			base::Box<::view::Metadata> getView() const;
		};

		class Info {
		private:
			Metadata                        metadata;
			std::shared_ptr<Component>      header;
			std::vector<base::Box<Section>> sections;

		public:
			Info(
				Metadata                        metadata,
				std::shared_ptr<Component>      header,
				std::vector<base::Box<Section>> sections
			);

			static Info createFromInfo(
				const message_template::Info&                       info,
				std::shared_ptr<ViewConstructor>&                   view_constructor,
				base::HashMap<component_id_t, component_context_t>& id_to_component_context
			);

			base::Box<::view::Info> getView() const;
		};

		class Diagnostic {
		private:
			std::vector<Info> infos;

		public:
			Diagnostic(std::vector<Info> infos);

			static Diagnostic createFromViewConstructor(
				std::shared_ptr<ViewConstructor>&                   view_constructor,
				base::HashMap<component_id_t, component_context_t>& id_to_component_context
			);

			base::Box<::view::Diagnostic> getView() const;
		};
	}  // namespace view_manager
}  // namespace dia_app
