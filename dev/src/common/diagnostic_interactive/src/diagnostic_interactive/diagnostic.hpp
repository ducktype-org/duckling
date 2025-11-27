#pragma once

#include <diagnostic_interactive/core/diagnostic_file.hpp>

#include <diagnostic/location.hpp>
#include <diagnostic/source_position.hpp>
#include <token_source/source.hpp>

#include <string>
#include <utility>

namespace dia_int {
	using namespace dia_app;

	class DiagnosticBase;

	/**
	 * The Argument and CodeArgument are represented by two different classes
	 * because they have different serialization logic.
	 */

	class Argument {
	private:
		std::string name;

	public:
		Argument(std::string name): name(std::move(name)) {}

		/**
		 * @warning This operation can change the DiagnosticBase - for example add new additional
		 * message
		 */
		virtual Box<dia_file::Component> getValue(DiagnosticBase&) = 0;

		[[nodiscard]] const std::string& getName() const { return name; }

		virtual ~Argument() = default;
	};

	class TextArgument: public Argument {
		std::string content;

	public:
		TextArgument(std::string name, std::string content):
			  Argument(std::move(name)),
			  content(std::move(content)) {}

		Box<dia_file::Component> getValue(DiagnosticBase&) override {
			return base::makeBox<dia_file::TextComponent>(content);
		}
	};

	class CodeArgument: public Argument {
		dia::SourcePosition position;
		usize               lines_before = 2;
		usize               lines_after  = 2;

	public:
		static void addCodeLines(
			std::vector<Box<dia_file::Component>>& code_list,
			Ref<tokenizer::TokenSource>            source,
			usize                                  start,
			usize                                  end
		);

		Box<dia_file::Component> getValue(DiagnosticBase&) override;

		CodeArgument(std::string name, dia::SourcePosition position):
			  Argument(std::move(name)),
			  position(position) {}
	};

	class CodeLocationArgument: public Argument {
		struct FileLocation {
			std::string file;
			u64         line;
			u64         column;

			static FileLocation fromSourcePosition(const dia::SourcePosition& pos) {
				auto [line, column] = pos.getStartLineColumn();
				return { .file   = pos.getSource()->getFile().getFilePath().string(),
					     .line   = (u64) line,
					     .column = (u64) column };
			}
		};

		FileLocation location;

	public:
		CodeLocationArgument(std::string name, FileLocation location):
			  Argument(std::move(name)),
			  location(std::move(location)) {}

		CodeLocationArgument(std::string name, dia::SourcePosition position):
			  Argument(std::move(name)),
			  location(FileLocation::fromSourcePosition(position)) {}

		Box<dia_file::Component> getValue(DiagnosticBase&) override {
			return base::makeBox<dia_file::CodeLocationComponent>(
				location.file, location.line, location.column
			);
		}
	};

	class TemplateComponentArgument {};

	class Entity {
	public:
		virtual dia_file::Entity getEntity() = 0;

		virtual ~Entity() = default;
	};

	class TypeEntity: public Entity {};

	class VariableEntity: public Entity {};

	class PointerMessage {
	public:
		std::string    name;
		dia::SourcePosition position;

		PointerMessage(std::string name, dia::SourcePosition position):
			  name(std::move(name)),
			  position(position) {}
	};

	class ExploreLink {};

	class AutomaticInteractivity {};

	class DiagnosticBase {
	private:
		std::vector<Box<Argument>>     arguments;
		std::vector<Box<Entity>>       entities;

		std::vector<PointerMessage> pointer_messages;
		std::vector<ExploreLink>    explore_links;

		std::vector<AutomaticInteractivity> automatic_interactivity;
		std::vector<Box<DiagnosticBase>>    attached_messages;

		/**
		 * @brief Additional messages that are not attached directly to this diagnostic,
		 * but may be the link destination or explore link target.
		 */
		base::HashMap<std::string, Box<DiagnosticBase>> related_diagnostics;
	protected:
		void addArgument(Box<Argument> param) { arguments.push_back(std::move(param)); }

		template<typename T, typename... Args>
		void addArgument(Args&&... args) {
			arguments.push_back(base::makeBox<T>(std::forward<Args>(args)...));
		}

		void addEntity(Box<Entity> entity) { entities.push_back(std::move(entity)); }

		void addPointerMessage(PointerMessage msg) { pointer_messages.push_back(std::move(msg)); }

		template<typename... Args>
		void addPointerMessage(Args&&... args) {
			pointer_messages.push_back(
				PointerMessage(std::forward<Args>(args)...)
			);
		}

		virtual dia_file::Metadata getMetadata() const = 0;

		dia_file::Thread constructThread() {
			dia_file::Thread thread;
			thread.main_message.metadata = getMetadata();
			for (const auto& arg: arguments)
				thread.main_message.arguments.put(arg->getName(), arg->getValue(*this));
			return thread;
		}

		DiagnosticBase() = default;

	public:
		dia_file::Thread buildDiagnosticFile() {
			return constructThread();
		}

		void addNote(Box<DiagnosticBase> note) { attached_messages.push_back(std::move(note)); }

		void addExploreLink(ExploreLink link) { explore_links.push_back(std::move(link)); }

		const std::vector<PointerMessage>& getPointerMessages() const {
			return pointer_messages;
		}


		virtual ~DiagnosticBase() = default;
	};
}
