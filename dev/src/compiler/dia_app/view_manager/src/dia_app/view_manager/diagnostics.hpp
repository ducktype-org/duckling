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

			base::Box<::view::HlMessage> getView(ViewConstructor& vc) const;
		};

		class Section {
		private:

		public:
			virtual ~Section();

			virtual base::Box<::view::Section> getView(ViewConstructor& vc) const;
		};

		class TextSection: public Section {
		private:
			std::shared_ptr<Component> root;

		public:
			TextSection(std::shared_ptr<Component> root);

			base::Box<::view::Section> getView(ViewConstructor& vc) const override;
		};

		class CodeSection: public Section {
		private:
			base::Box<CodeMetadata>    code_metadata;
			std::shared_ptr<Component> root;
			std::vector<HlMessage>     hl_messages;

		public:
			CodeSection(
				base::Box<CodeMetadata>    code_metadata,
				std::shared_ptr<Component> root,
				std::vector<HlMessage>     hl_messages
			);

			static base::Optional<base::Box<CodeSection>> createFromInfo(
				const message_template::Info&              info,
				ViewConstructor&                           view_constructor,
				const std::function<hl_id_t(std::string)>& hl_name_to_id,
				const std::function<u32()>&                get_next_id,
				const std::function<hl_id_t(std::string)>& group_to_id
			);

			base::Box<::view::Section> getView(ViewConstructor& vc) const override;
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
				const message_template::Info&              info,
				ViewConstructor&                           view_constructor,
				const std::function<hl_id_t(std::string)>& hl_name_to_id,
				const std::function<u32()>&                get_next_id,
				const std::function<hl_id_t(std::string)>& group_to_id
			);

			base::Box<::view::Info> getView(ViewConstructor& vc) const;
		};

		class Diagnostic {
		private:
			std::vector<Info> infos;

		public:
			Diagnostic(std::vector<Info> infos);

			static Diagnostic createFromViewConstructor(
				ViewConstructor&                           view_constructor,
				const std::function<hl_id_t(std::string)>& hl_name_to_id,
				const std::function<u32()>&                get_next_id,
				const std::function<hl_id_t(std::string)>& group_to_id
			);

			base::Box<::view::Diagnostic> getView(ViewConstructor& vc) const;
		};
	}  // namespace view_manager
}  // namespace dia_app
