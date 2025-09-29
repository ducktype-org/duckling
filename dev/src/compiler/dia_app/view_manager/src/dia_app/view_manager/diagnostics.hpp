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

		/**
		 * @brief Pointer message attached to a highlight group within a code section.
		 */
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

			/**
			 * @brief Serialize to gRPC view pointer message, using styles inferred
			 * from InfoType and resolving lazy content via ViewConstructor.
			 */
			base::Box<::view::HlMessage> getView(ViewConstructor& vc) const;
		};

		class Section {
		private:

		public:
			virtual ~Section();

			virtual base::Box<::view::Section> getView(ViewConstructor& vc) const;
		};

		/**
		 * @brief Text-only section rendered without highlight metadata.
		 */
		class TextSection: public Section {
		private:
			std::shared_ptr<Component> root;

		public:
			TextSection(std::shared_ptr<Component> root);

			base::Box<::view::Section> getView(ViewConstructor& vc) const override;
		};

		/**
		 * @brief Code section with location, content, and pointer messages.
		 *
		 * Pointer messages are assigned a priority to guide placement; lower
		 * numbers indicate higher priority.
		 */
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

			/**
			 * @brief Construct a CodeSection from a templated Info if present.
			 *
			 * Returns empty if the info does not contain a code section.
			 * Performs conversion of display elements to components and collects
			 * pointer messages with priorities and types.
			 */
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

		/**
		 * @brief Info metadata for serialization (type and numeric code).
		 */
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

			/**
			 * @brief Build an Info from a templated Info using conversion helpers.
			 *
			 * Converts display elements into component trees and collects pointer
			 * messages and code sections if present.
			 */
			static Info createFromInfo(
				const message_template::Info&              info,
				ViewConstructor&                           view_constructor,
				const std::function<hl_id_t(std::string)>& hl_name_to_id,
				const std::function<u32()>&                get_next_id,
				const std::function<hl_id_t(std::string)>& group_to_id
			);

			base::Box<::view::Info> getView(ViewConstructor& vc) const;
		};

		/**
		 * @brief Collection of infos displayed together as one diagnostic group.
		 */
		class Diagnostic {
		private:
			std::vector<Info> infos;

		public:
			Diagnostic(std::vector<Info> infos);

			/**
			 * @brief Build a Diagnostic from a ViewConstructor by evaluating
			 * main and secondary infos and converting them into sections.
			 */
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
