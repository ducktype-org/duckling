#include "diagnostics.hpp"

namespace dia_app {
	namespace view_manager {
		// CodeMetadata

		CodeMetadata::CodeMetadata(std::string filename, line_no_t line, column_no_t column):
			  filename(std::move(filename)),
			  line(line),
			  column(column) {}

		std::unique_ptr<CodeMetadata> CodeMetadata::createFromLocation(
			const dia_file::CodeData::Location& location
		) {
			return std::make_unique<CodeMetadata>(location.file, location.line, location.column);
		}

		std::unique_ptr<::view::CodeMetadata> CodeMetadata::getView() const {
			auto result = std::make_unique<::view::CodeMetadata>();
			result->set_filename(this->filename);
			result->set_line(this->line);
			result->set_column(this->column);
			return result;
		}

		// HlMessage

		HlMessage::HlMessage(
			hl_id_t tag, priority_t priority, InfoType type, std::shared_ptr<Component> content
		):
			  tag(tag),
			  priority(priority),
			  type(type),
			  content(std::move(content)) {}

		::view::InfoType toProtocol(InfoType type) {
			switch (type) {
			case InfoType::Error:
				return ::view::InfoType::Error;
			case InfoType::Warning:
				return ::view::InfoType::Warning;
			case InfoType::Note:
				return ::view::InfoType::Note;
			case InfoType::Hint:
				return ::view::InfoType::Hint;
			case InfoType::Docs:
				return ::view::InfoType::Docs;
			}
		}

		std::unique_ptr<::view::HlMessage> HlMessage::getView() const {
			auto result = std::make_unique<::view::HlMessage>();
			result->set_tag(this->tag);
			result->set_priority(this->priority);
			result->set_type(toProtocol(this->type));
			auto components = filterEmptyLines(this->content->getNoHlView());
			for (auto& elm: components) {
				result->mutable_message()
					->mutable_concat_component()
					->mutable_components()
					->AddAllocated(elm.release());
			}
			return result;
		}

		// Section

		Section::~Section() {}

		std::unique_ptr<::view::Section> Section::getView() const {
			assert(false);
			return nullptr;
		}

		// TextSection

		TextSection::TextSection(std::shared_ptr<Component> root): root(std::move(root)) {}

		std::unique_ptr<::view::Section> TextSection::getView() const {
			debug("TextSection::getView() begin");
			auto result       = std::make_unique<::view::Section>();
			auto text_section = concatNoHlLines(filterEmptyLines(this->root->getNoHlView()));
			result->set_allocated_text_section(text_section.release());
			debug("TextSection::getView() end");
			return result;
		}

		// CodeSection

		CodeSection::CodeSection(
			CodeMetadata               code_metadata,
			std::shared_ptr<Component> root,
			std::vector<HlMessage>     hl_messages
		):
			  code_metadata(std::move(code_metadata)),
			  root(std::move(root)),
			  hl_messages(std::move(hl_messages)) {}

		std::unique_ptr<CodeSection> CodeSection::createFromInfo(
			const message_template::Info& info, std::shared_ptr<CreationContext> creation_context
		) {
			if (!info.code.has_value()) return {};
			auto& code = info.code.value();

			auto code_metadata = CodeMetadata::createFromLocation(code.location);

			// Mapping from highlight names is created before processing the root component
			std::vector<HlMessage> hl_messages;
			{
				hl_id_t id = 0;
				for (const auto& [name, pointer_message]: info.pointer_messages) {
					++id;
					creation_context->hl_name_to_id->emplace(name, id);
					hl_messages.emplace_back(
						id,
						pointer_message.priority,
						from_string(pointer_message.type),
						pointer_message.message->toComponent(creation_context)
					);
				}
			}

			auto root = code.content->toComponent(creation_context);

			return std::make_unique<CodeSection>(
				*code_metadata.release(), std::move(root), std::move(hl_messages)
			);
		}

		std::unique_ptr<::view::Section> CodeSection::getView() const {
			auto result = std::make_unique<::view::Section>();
			{
				auto metadata = this->code_metadata.getView();
				result->mutable_code_section()->set_allocated_metadata(metadata.release());
			}
			{
				debug("CodeSection::getView() begin");
				auto [suffix, mid] = this->root->getHlView();
				debug(ssize(mid));
				std::vector<std::unique_ptr<::view::CodeLine>> lines;
				if (suffix.has_value()) {
					auto line = std::make_unique<::view::CodeLine>();
					line->set_allocated_content(suffix.value().release());
					lines.emplace_back(std::move(line));
				}

				auto mid_lines
					= std::ranges::subrange(mid.begin(), mid.end())
				    | std::views::transform([](line_data_t<::view::HlComponent>& line_data) {
						  auto& [line_number, component] = line_data;
						  auto line                      = std::make_unique<::view::CodeLine>();
						  if (line_number.has_value())
							  line->set_line_number(line_no_t(line_number.value()));
						  if (component.has_value())
							  line->set_allocated_content(component.value().release());
						  return line;
					  });
				lines.insert(
					lines.end(),
					std::make_move_iterator(mid_lines.begin()),
					std::make_move_iterator(mid_lines.end())
				);
				debug(ssize(lines));

				for (auto& line: lines)
					result->mutable_code_section()->mutable_lines()->AddAllocated(line.release());
			}
			{
				std::vector<std::unique_ptr<::view::HlMessage>> hl_message_view(
					ssize(this->hl_messages)
				);
				std::transform(
					this->hl_messages.begin(),
					this->hl_messages.end(),
					hl_message_view.begin(),
					[](const HlMessage& hl_info) { return hl_info.getView(); }
				);
				for (auto& elm: hl_message_view) {
					result->mutable_code_section()->mutable_hl_messages()->AddAllocated(elm.release(
					));
				}
			}
			debug("CodeSection::getView() end");
			return result;
		}

		// Metadata
		Metadata::Metadata(InfoType type, error_code_t code): type(type), code(code) {}

		Metadata Metadata::createFromInfo(const message_template::Info& info) {
			return Metadata(info.metadata.type, info.metadata.code);
		}

		std::unique_ptr<::view::Metadata> Metadata::getView() const {
			debug("Metadata::getView() begin");
			auto type = [this]() {
				switch (this->type) {
				case InfoType::Error:
					return ::view::InfoType::Error;
				case InfoType::Warning:
					return ::view::InfoType::Warning;
				case InfoType::Note:
					return ::view::InfoType::Note;
				case InfoType::Hint:
					return ::view::InfoType::Hint;
				case InfoType::Docs:
					return ::view::InfoType::Docs;
				}
				assert(false);
			}();
			auto result = std::make_unique<::view::Metadata>();
			result->set_type(type);
			result->set_code(this->code);
			debug("Metadata::getView() end");
			return result;
		}

		// Info
		Info::Info(
			Metadata                              metadata,
			std::shared_ptr<Component>            header,
			std::vector<std::unique_ptr<Section>> sections
		):
			  metadata(metadata),
			  header(std::move(header)),
			  sections(std::move(sections)) {}

		Info Info::createFromInfo(
			const message_template::Info& info,
			std::shared_ptr<CreationContext> creation_context
		) {
			auto                                  metadata = Metadata::createFromInfo(info);
			std::vector<std::unique_ptr<Section>> sections;
			assert(info.header_message);
			auto header       = info.header_message->toComponent(creation_context);
			auto code_section = CodeSection::createFromInfo(info, creation_context);
			if (code_section) sections.emplace_back(std::move(code_section));
			if (info.description) {
				sections.emplace_back(
					std::make_unique<TextSection>(info.description->toComponent(creation_context))
				);
			}
			debug("Number of sections: ", ssize(sections));
			return Info(metadata, std::move(header), std::move(sections));
		}

		std::unique_ptr<::view::Info> Info::getView() const {
			auto info = std::make_unique<::view::Info>();
			{
				auto metadata = this->metadata.getView();
				info->set_allocated_metadata(metadata.release());
			}
			{
				auto header = concatNoHlLines(filterEmptyLines(this->header->getNoHlView()));
				info->set_allocated_header(header.release());
			}
			{
				std::vector<std::unique_ptr<::view::Section>> section_view(ssize(this->sections));
				std::transform(
					sections.begin(),
					sections.end(),
					section_view.begin(),
					[](const std::unique_ptr<Section>& section) { return section->getView(); }
				);
				debug("While adding view sections: allocated sections cnt: ", ssize(section_view));
				for (auto& elm: section_view) {
					if (elm != nullptr) {
						debug("non-nullptr");
						info->mutable_sections()->AddAllocated(elm.release());
					} else {
						debug("nullptr");
					}
				}
			}
			return info;
		}

		// Diagnostic

		Diagnostic::Diagnostic(std::vector<Info> infos): infos(std::move(infos)) {}

		Diagnostic Diagnostic::createFromViewConstructor(
			std::shared_ptr<ViewConstructor>& view_constructor,
			std::shared_ptr<CreationContext> creation_context
		) {
			std::vector<Info>            infos;
			const message_template::Info info = *view_constructor->loadMainInfo();
			infos.emplace_back(Info::createFromInfo(info, creation_context));
			for (auto info_handle: view_constructor->displayed_secondary_infos) {
				infos.emplace_back(Info::createFromInfo(
					*view_constructor->loadSecondaryInfo(info_handle), creation_context
				));
			}
			return Diagnostic(std::move(infos));
		}

		std::unique_ptr<::view::Diagnostic> Diagnostic::getView() const {
			debug("Diagnostic::getView() begin");
			auto diagnostic = std::make_unique<::view::Diagnostic>();
			std::vector<std::unique_ptr<::view::Info>> info_view(ssize(this->infos));
			std::transform(
				this->infos.begin(),
				this->infos.end(),
				info_view.begin(),
				[](const Info& info) { return info.getView(); }
			);
			for (auto& info: info_view) diagnostic->mutable_infos()->AddAllocated(info.release());
			debug("Diagnostic::getView() end");
			return diagnostic;
		}
	}  // namespace view_manager
}  // namespace dia_app
