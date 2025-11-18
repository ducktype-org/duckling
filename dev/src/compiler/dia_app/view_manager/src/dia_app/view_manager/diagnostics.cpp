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
			PointerMessageID tag, priority_t priority, InfoType type, std::shared_ptr<Component> content
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
			std::vector<line_data_t<T>> result;
			// Prefix content (before the first explicit line start)
			if (data.first.has_value())
				result.emplace_back(line_metadata_t{}, std::move(data.first.value()));
			// Mid lines: keep only non-empty components
			for (auto& [meta, comp]: data.second)
				if (comp.has_value()) result.emplace_back(meta, std::move(comp.value()));
			return result;
		}

		base::Box<::view::HlMessage> HlMessage::getView(ViewConstructor& vc) const {
			auto result = base::makeBox<::view::HlMessage>();
			result->set_tag(this->tag);
			result->set_priority(this->priority);
			result->set_type(toProtocol(this->type));
			GetNoHlViewVisitor visitor    = GetNoHlViewVisitor(vc);
			auto               components = filterEmptyLines(this->content->accept(visitor));
			// TODO: To be resolved what type we want from filterEmptyLines. Currently we just need
			// a list of components. IDK if it's needed to filter here...
			for (auto& elm: components) {
				if (elm.second.has_value()) {
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

		base::Box<::view::Section> Section::getView(ViewConstructor& vc) const { assert(false); }

		// TextSection

		TextSection::TextSection(std::shared_ptr<Component> root): root(std::move(root)) {}

		base::Box<::view::NoHlComponent> concatNoHlLines(
			std::vector<line_data_t<::view::NoHlComponent>> lines
		) {
			// Build a single NoHl concat component with newlines between lines
			auto  result   = base::makeBox<::view::NoHlComponent>();
			auto* repeated = result->mutable_concat_component()->mutable_components();
			bool  first    = true;
			for (auto& [ignored_meta, component_opt]: lines) {
				if (!component_opt.has_value()) continue;
				if (!first) {
					// Insert a newline between non-empty lines
					auto newline = base::makeBox<::view::NoHlComponent>();
					newline->mutable_text_component()->set_content("\n");
					repeated->AddAllocated(newline.release());
				}
				first = false;
				// Append the actual line content
				repeated->AddAllocated(component_opt.value().release());
			}
			return result;
		}

		base::Box<::view::Section> TextSection::getView(ViewConstructor& vc) const {
			auto               result  = base::makeBox<::view::Section>();
			GetNoHlViewVisitor visitor = GetNoHlViewVisitor(vc);
			auto text_section = concatNoHlLines(filterEmptyLines(this->root->accept(visitor)));
			result->set_allocated_text_section(text_section.release());
			return result;
		}

		// CodeSection

		CodeSection::CodeSection(
			base::Box<CodeMetadata>    code_metadata,
			std::shared_ptr<Component> root,
			std::vector<HlMessage>     hl_messages
		):
			  code_metadata(std::move(code_metadata)),
			  root(std::move(root)),
			  hl_messages(std::move(hl_messages)) {}

		base::Optional<base::Box<CodeSection>> CodeSection::createFromInfo(
			const message_template::Info&              info,
			ViewConstructor&                           view_constructor,
			const std::function<PointerMessageID(std::string)>& hl_name_to_id,
			const std::function<u32()>&                get_next_id,
			const std::function<PointerMessageID(std::string)>& group_to_id
		) {
			if (!info.code.has_value()) return {};
			auto& code = info.code.value();

			auto code_metadata = CodeMetadata::createFromLocation(code.location);

			// Mapping from highlight names is created before processing the root component
			std::vector<HlMessage> hl_messages;
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
				view_constructor, dia_file::DisplayElement::AccData(), get_next_id, group_to_id
			);
			auto root = code.content->accept(visitor);

			return base::makeBox<CodeSection>(
				std::move(code_metadata), std::move(root), std::move(hl_messages)
			);
		}

		base::Box<::view::Section> CodeSection::getView(ViewConstructor& vc) const {
			auto result = base::makeBox<::view::Section>();
			{
				auto metadata = this->code_metadata->getView();
				result->mutable_code_section()->set_allocated_metadata(metadata.release());
			}
			{
				GetHlViewVisitor visitor = GetHlViewVisitor(vc);
				auto [suffix, mid]       = this->root->accept(visitor);
				std::vector<base::Box<::view::CodeLine>> lines;
				if (suffix.has_value()) {
					auto line = base::makeBox<::view::CodeLine>();
					line->set_allocated_content(suffix.value().release());
					lines.emplace_back(std::move(line));
				}

				auto mid_lines
					= std::ranges::subrange(mid.begin(), mid.end())
				    | std::views::transform([](line_data_t<::view::HlComponent>& line_data) {
						  auto& [line_number, component] = line_data;
						  auto line                      = base::makeBox<::view::CodeLine>();
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
				for (const auto& hl_info: this->hl_messages) {
					result->mutable_code_section()->mutable_hl_messages()->AddAllocated(
						hl_info.getView(vc).release()
					);
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
			Metadata                        metadata,
			std::shared_ptr<Component>      header,
			std::vector<base::Box<Section>> sections
		):
			  metadata(metadata),
			  header(std::move(header)),
			  sections(std::move(sections)) {}

		Info Info::createFromInfo(
			const message_template::Info&              info,
			ViewConstructor&                           view_constructor,
			const std::function<PointerMessageID(std::string)>& hl_name_to_id,
			const std::function<u32()>&                get_next_id,
			const std::function<PointerMessageID(std::string)>& group_to_id
		) {
			auto                            metadata = Metadata::createFromInfo(info);
			std::vector<base::Box<Section>> sections;
			assert(info.header_message);
			dia_file::ToComponentVisitor visitor = dia_file::ToComponentVisitor(
				view_constructor, dia_file::DisplayElement::AccData(), get_next_id, group_to_id

			);
			auto header       = info.header_message->accept(visitor);
			auto code_section = CodeSection::createFromInfo(
				info, view_constructor, hl_name_to_id, get_next_id, group_to_id
			);
			if (code_section.has_value()) sections.emplace_back(std::move(code_section.value()));
			if (info.description) {
				dia_file::ToComponentVisitor visitor = dia_file::ToComponentVisitor(
					view_constructor, dia_file::DisplayElement::AccData(), get_next_id, group_to_id
				);
				sections.emplace_back(base::makeBox<TextSection>(info.description->accept(visitor)));
			}
			return Info(metadata, std::move(header), std::move(sections));
		}

		base::Box<::view::Info> Info::getView(ViewConstructor& vc) const {
			auto info = base::makeBox<::view::Info>();
			{
				auto metadata = this->metadata.getView();
				info->set_allocated_metadata(metadata.release());
			}
			{
				GetNoHlViewVisitor visitor = GetNoHlViewVisitor(vc);
				auto header = concatNoHlLines(filterEmptyLines(this->header->accept(visitor)));
				info->set_allocated_header(header.release());
			}
			{
				for (const auto& section: this->sections)
					info->mutable_sections()->AddAllocated(section->getView(vc).release());
			}
			return info;
		}

		// Diagnostic

		Diagnostic::Diagnostic(std::vector<Info> infos): infos(std::move(infos)) {}

		Diagnostic Diagnostic::createFromViewConstructor(
			ViewConstructor&                           view_constructor,
			const std::function<PointerMessageID(std::string)>& hl_name_to_id,
			const std::function<u32()>&                get_next_id,
			const std::function<PointerMessageID(std::string)>& group_to_id
		) {
			std::vector<Info> infos;
			infos.emplace_back(Info::createFromInfo(
				view_constructor.getMainInfo(), view_constructor, hl_name_to_id, get_next_id, group_to_id
			));
			auto secondary_infos = view_constructor.getDisplayedSecondaryInfos();
			for (const auto& info: secondary_infos)
				infos.emplace_back(Info::createFromInfo(
					info, view_constructor, hl_name_to_id, get_next_id, group_to_id
				));
			return Diagnostic(std::move(infos));
		}

		base::Box<::view::Diagnostic> Diagnostic::getView(ViewConstructor& vc) const {
			auto                                 diagnostic = base::makeBox<::view::Diagnostic>();
			std::vector<base::Box<::view::Info>> info_view;
			for (const auto& info: this->infos) info_view.emplace_back(info.getView(vc));
			for (auto& info: info_view) diagnostic->mutable_infos()->AddAllocated(info.release());
			return diagnostic;
		}
	}  // namespace view_manager
}  // namespace dia_app
