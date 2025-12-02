#pragma once

#include <diagnostic_interactive/core/diagnostic_file_forward.hpp>

#include <diagnostic/location.hpp>
#include <diagnostic/source_position.hpp>
#include <token_source/source.hpp>

#include <string>
#include <utility>
#include <vector>

namespace dia_int {

	class MessageBase;

	struct Metadata {
		std::string template_type;
		std::string type;
		std::string family;
		std::string name;

		operator dia_file::Metadata() const;
	};

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
		virtual Box<dia_file::Component> getValue(MessageBase&) = 0;

		[[nodiscard]] const std::string& getName() const { return name; }

		virtual ~Argument() = default;
	};

	class TextArgument: public Argument {
		std::string content;

	public:
		TextArgument(std::string name, std::string content):
			  Argument(std::move(name)),
			  content(std::move(content)) {}

		Box<dia_file::Component> getValue(MessageBase&) override;
	};

	class CodeArgument: public Argument {
		dia::SourcePosition position;
		usize               lines_before = 1;
		usize               lines_after  = 1;

	public:
		static void addCodeLines(
			std::vector<Box<dia_file::Component>>& code_list,
			Ref<tokenizer::TokenSource>            source,
			usize                                  start,
			usize                                  end
		);

		Box<dia_file::Component> getValue(MessageBase&) override;

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

		Box<dia_file::Component> getValue(MessageBase&) override;
	};

	class TemplateComponentArgument {};

	class InteractiveElement {
	public:
		virtual Box<dia_file::Component> getValue(MessageBase&) = 0;

		virtual ~InteractiveElement() = default;
	};

	class InteractiveArgument: public Argument {
	private:
		Box<InteractiveElement> element;

	public:
		InteractiveArgument(std::string name, Box<InteractiveElement> element):
			  Argument(std::move(name)),
			  element(std::move(element)) {}

		Box<dia_file::Component> getValue(MessageBase& message) override {
			return element->getValue(message);
		}
	};

	class Entity {
		std::vector<std::string> linked_messages;

	public:
		[[nodiscard]] const std::vector<std::string>& getLinkedMessages() const {
			return linked_messages;
		}

		Entity(std::vector<std::string> linked_messages):
			  linked_messages(std::move(linked_messages)) {}

		virtual ~Entity() = default;
	};

	class TextBasedEntity: public Entity {
		std::string displayed_name;

	public:
		TextBasedEntity(std::vector<std::string> linked_messages, std::string displayed_name):
			  Entity(std::move(linked_messages)),
			  displayed_name(std::move(displayed_name)) {}

		[[nodiscard]] const std::string& getDisplayedName() const { return displayed_name; }
	};

	class PointerMessage {
	public:
		std::string                 name;
		dia::SourcePosition         position;
		base::Optional<std::string> message_id;

		PointerMessage(std::string name, dia::SourcePosition position, 
		               base::Optional<std::string> message_id = {}):
			  name(std::move(name)),
			  position(position),
			  message_id(std::move(message_id)) {}
	};

	class ExploreLink {
		std::string                message_id;
		std::vector<Box<Argument>> arguments;

	public:
		ExploreLink(std::string message_id, std::vector<Box<Argument>> arguments):
			  message_id(std::move(message_id)),
			  arguments(std::move(arguments)) {}

		dia_file::ExploreEdge getValue(MessageBase& message) const;
	};

	class MessageBase {
	private:
		std::vector<Box<Argument>> arguments;
		std::vector<Box<Entity>>   entities;

		std::vector<PointerMessage> pointer_messages;
		std::vector<ExploreLink>    explore_links;

		std::vector<Box<MessageBase>> attached_messages;

		/**
		 * @brief Additional messages that are not attached directly to this diagnostic,
		 * but may be the link destination or explore link target.
		 */
		base::HashMap<std::string, Box<MessageBase>> linked_messages;

		virtual Metadata getMetadata() const = 0;


		dia_file::Message buildMessages(
			base::HashMap<std::string, dia_file::Message>& additional_messages
		);

	protected:
		MessageBase() = default;

	public:
		/* =============================  MODIFIERS ============================= */
		/**
		 * @note The methods below should not be used directly.
		 * They are intended to be used by derived classes and InteractiveArguments only.
		 */
		void addArgument(Box<Argument> param) { arguments.push_back(std::move(param)); }

		template<typename T, typename... Args>
		void addArgument(Args&&... args) {
			arguments.push_back(base::makeBox<T>(std::forward<Args>(args)...));
		}

		void addPointerMessage(PointerMessage msg) { pointer_messages.push_back(std::move(msg)); }

		template<typename... Args>
		void addPointerMessage(Args&&... args) {
			pointer_messages.push_back(PointerMessage(std::forward<Args>(args)...));
		}

		void addEntity(Box<Entity> entity) { entities.push_back(std::move(entity)); }

		template<typename T, typename... Args>
		void addEntity(Args&&... args) {
			entities.push_back(base::makeBox<T>(std::forward<Args>(args)...));
		}

		void addExploreLink(ExploreLink link) { explore_links.push_back(std::move(link)); }

		template<typename... Args>
		void addExploreLink(Args&&... args) {
			explore_links.push_back(ExploreLink(std::forward<Args>(args)...));
		}

		void addLinkedMessage(std::string id, Box<MessageBase> message) {
			linked_messages.insertOrAssign(std::move(id), std::move(message));
		}

		void appendMessage(Box<MessageBase> note) { attached_messages.push_back(std::move(note)); }

		/* =============================  ACCESSORS ============================= */

		/**
		 * @note These method is used by the Parameter to get the pointer messages on the code.
		 */
		const std::vector<PointerMessage>& getPointerMessages() const { return pointer_messages; }

		/**
		 * @brief Main method that builds the diagnostic file representation of this diagnostic.
		 */
		Box<dia_file::Thread> buildDiagnosticFile();

		virtual ~MessageBase() = default;

		static std::string getUniqueID();
	};

}
