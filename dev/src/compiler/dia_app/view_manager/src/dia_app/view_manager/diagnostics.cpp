#include "diagnostics.hpp"
#include "display_elements.hpp"

namespace dia_app {
	namespace view_manager {
		// CodeMetadata

		CodeMetadata::CodeMetadata(std::string filename, line_no_t line, column_no_t column):
			  filename(std::move(filename)),
			  line(line),
			  column(column) {}

		base::Box<CodeMetadata> CodeMetadata::createFromLocation(
			const dia_file::CodeData::Location& location
		) {
			return base::makeBox<CodeMetadata>(location.file, location.line, location.column);
		}

		base::Box<::view::CodeMetadata> CodeMetadata::getView() const {
			auto result = base::makeBox<::view::CodeMetadata>();
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

        template<class T>
        std::vector<line_data_t<T>> filterEmptyLines(component_get_view_data_t<T> data) {
            UNIMPLEMENTED();
        }

		base::Box<::view::HlMessage> HlMessage::getView(ViewConstructor &vc) const {
			auto result = base::makeBox<::view::HlMessage>();
			result->set_tag(this->tag);
			result->set_priority(this->priority);
			result->set_type(toProtocol(this->type));
            GetNoHlViewVisitor visitor = GetNoHlViewVisitor(vc);
			auto components = filterEmptyLines(this->content->accept(visitor));
            // TODO: To be resolved what type we want from filterEmptyLines. Currently we just need a list of components.
            // IDK if it's needed to filter here...
			for (auto& elm: components) {
                if (
                    elm.second.has_value()
                ) {
                    result->mutable_message()
                        ->mutable_concat_component()
                        ->mutable_components()
                        ->AddAllocated(elm.second.value().release());
                }
			}
			return result;
		}

		// Section

		Section::~Section() {}

		base::Box<::view::Section> Section::getView(ViewConstructor &vc) const {
			assert(false);
		}

		// TextSection

		TextSection::TextSection(std::shared_ptr<Component> root): root(std::move(root)) {}

        std::unique_ptr<::view::NoHlComponent> concatNoHlLines(std::vector<line_data_t<::view::NoHlComponent>> lines) {
            UNIMPLEMENTED();
        }

		base::Box<::view::Section> TextSection::getView(ViewConstructor &vc) const {
			auto result       = base::makeBox<::view::Section>();
            GetNoHlViewVisitor visitor = GetNoHlViewVisitor(vc);
			auto text_section = concatNoHlLines(filterEmptyLines(this->root->accept(visitor)));
			result->set_allocated_text_section(text_section.release());
			return result;
		}

		// CodeSection

		CodeSection::CodeSection(
			base::Box<CodeMetadata>               code_metadata,
			std::shared_ptr<Component> root,
			std::vector<HlMessage>     hl_messages
		):
			  code_metadata(std::move(code_metadata)),
			  root(std::move(root)),
			  hl_messages(std::move(hl_messages)) {}

		base::Optional<base::Box<CodeSection>> CodeSection::createFromInfo(
				const message_template::Info&                       info,
				ViewConstructor&                   view_constructor,
				base::HashMap<component_id_t, component_context_t>& id_to_component_context,
                std::function<hl_id_t(std::string)> hl_name_to_id
		) {
			if (!info.code.has_value()) return {};
			auto& code = info.code.value();

			auto code_metadata = CodeMetadata::createFromLocation(code.location);

			// Mapping from highlight names is created before processing the root component
			std::vector<HlMessage> hl_messages;
            hl_id_t next_component_id = 0;
            std::function<u32()>                              get_next_id = [&next_component_id]() {
                return next_component_id++;
            };
            base::HashMap<std::string, hl_id_t> group_to_id_map; 
            hl_id_t next_group_id = 0;
            std::function<view_manager::hl_id_t(std::string)> group_to_id = [&group_to_id_map, &next_group_id](std::string str) {
                auto ptr = group_to_id_map.find(str);
                if (ptr == group_to_id_map.end()) {
                    group_to_id_map[str] = next_group_id;
                    next_group_id++;
                }
                return group_to_id_map[str];
            };
			{
				for (const auto& [name, pointer_message]: info.pointer_messages) {
                    dia_file::ToComponentVisitor visitor = dia_file::ToComponentVisitor(
                        view_constructor,
                        dia_file::DisplayElement::AccData(),
                        get_next_id,
                        group_to_id
                    );
					hl_messages.emplace_back(
						hl_name_to_id(name),
						pointer_message.priority,
						pointer_message.type,
						pointer_message.message->accept(visitor)
					);
				}
			}
            dia_file::ToComponentVisitor visitor = dia_file::ToComponentVisitor(
                        view_constructor,
                        dia_file::DisplayElement::AccData(),
                        get_next_id,
                        group_to_id
                    );
			auto root = code.content->accept(visitor);

			return base::makeBox<CodeSection>(std::move(code_metadata), std::move(root), std::move(hl_messages));
		}

		base::Box<::view::Section> CodeSection::getView(ViewConstructor &vc) const {
			auto result = base::makeBox<::view::Section>();
			{
				auto metadata = this->code_metadata->getView();
				result->mutable_code_section()->set_allocated_metadata(metadata.release());
			}
			{
                GetHlViewVisitor visitor = GetHlViewVisitor(vc);
				auto [suffix, mid] = this->root->accept(visitor);
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
					[&vc](const HlMessage& hl_info) { return hl_info.getView(vc); }
				);
				for (auto& elm: hl_message_view) {
					result->mutable_code_section()->mutable_hl_messages()->AddAllocated(elm.release(
					));
				}
			}
			return result;
		}

		// Metadata
		Metadata::Metadata(InfoType type, error_code_t code): type(type), code(code) {}

		Metadata Metadata::createFromInfo(const message_template::Info& info) {
			return Metadata(info.metadata.type, info.metadata.code);
		}

		base::Box<::view::Metadata> Metadata::getView() const {
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
			auto result = base::makeBox<::view::Metadata>();
			result->set_type(type);
			result->set_code(this->code);
			return result;
		}

		// Info
		Info::Info(
			Metadata                              metadata,
			std::shared_ptr<Component>            header,
			std::vector<base::Box<Section>> sections
		):
			  metadata(metadata),
			  header(std::move(header)),
			  sections(std::move(sections)) {}

		Info Info::createFromInfo(
				const message_template::Info&                       info,
				std::shared_ptr<ViewConstructor>&                   view_constructor,
				base::HashMap<component_id_t, component_context_t>& id_to_component_context
		) {
            UNIMPLEMENTED();
		// 	auto                                  metadata = Metadata::createFromInfo(info);
		// 	std::vector<std::unique_ptr<Section>> sections;
		// 	assert(info.header_message);
		// 	auto header       = info.header_message->toComponent(creation_context);
		// 	auto code_section = CodeSection::createFromInfo(info, creation_context);
		// 	if (code_section) sections.emplace_back(std::move(code_section));
		// 	if (info.description) {
		// 		sections.emplace_back(
		// 			std::make_unique<TextSection>(info.description->toComponent(creation_context))
		// 		);
		// 	}
		// 	debug("Number of sections: ", ssize(sections));
		// 	return Info(metadata, std::move(header), std::move(sections));
		// }

		// std::unique_ptr<::view::Info> Info::getView() const {
		// 	auto info = std::make_unique<::view::Info>();
		// 	{
		// 		auto metadata = this->metadata.getView();
		// 		info->set_allocated_metadata(metadata.release());
		// 	}
		// 	{
		// 		auto header = concatNoHlLines(filterEmptyLines(this->header->getNoHlView()));
		// 		info->set_allocated_header(header.release());
		// 	}
		// 	{
		// 		std::vector<std::unique_ptr<::view::Section>> section_view(ssize(this->sections));
		// 		std::transform(
		// 			sections.begin(),
		// 			sections.end(),
		// 			section_view.begin(),
		// 			[](const std::unique_ptr<Section>& section) { return section->getView(); }
		// 		);
		// 		debug("While adding view sections: allocated sections cnt: ", ssize(section_view));
		// 		for (auto& elm: section_view) {
		// 			if (elm != nullptr) {
		// 				debug("non-nullptr");
		// 				info->mutable_sections()->AddAllocated(elm.release());
		// 			} else {
		// 				debug("nullptr");
		// 			}
		// 		}
		// 	}
		// 	return info;
		}

		// Diagnostic

		Diagnostic::Diagnostic(std::vector<Info> infos): infos(std::move(infos)) {}

		Diagnostic Diagnostic::createFromViewConstructor(
				std::shared_ptr<ViewConstructor>&                   view_constructor,
				base::HashMap<component_id_t, component_context_t>& id_to_component_context
		) {
            UNIMPLEMENTED();
			// std::vector<Info>            infos;
			// const message_template::Info info = *view_constructor->loadMainInfo();
			// infos.emplace_back(Info::createFromInfo(info, creation_context));
			// for (auto info_handle: view_constructor->displayed_secondary_infos) {
			// 	infos.emplace_back(Info::createFromInfo(
			// 		*view_constructor->loadSecondaryInfo(info_handle), creation_context
			// 	));
			// }
			// return Diagnostic(std::move(infos));
		}

		base::Box<::view::Diagnostic> Diagnostic::getView() const {
            UNIMPLEMENTED();
			// debug("Diagnostic::getView() begin");
			// auto diagnostic = std::make_unique<::view::Diagnostic>();
			// std::vector<std::unique_ptr<::view::Info>> info_view(ssize(this->infos));
			// std::transform(
			// 	this->infos.begin(),
			// 	this->infos.end(),
			// 	info_view.begin(),
			// 	[](const Info& info) { return info.getView(); }
			// );
			// for (auto& info: info_view) diagnostic->mutable_infos()->AddAllocated(info.release());
			// debug("Diagnostic::getView() end");
			// return diagnostic;
		}
	}  // namespace view_manager
}  // namespace dia_app
