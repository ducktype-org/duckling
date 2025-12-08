#pragma once

#include <diagnostic_interactive/core/diagnostic_arguments_forward.hpp>

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

		operator dia_args::Metadata() const;
	};

	/**
	 * @brief The base class for message arguments for the template evaluation.
	 * The specializations should create dia_args::Component when requested.
	 * They have a mandatory name.
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
		virtual Box<dia_args::Component> getValue(MessageBase&) = 0;

		[[nodiscard]] const std::string& getName() const { return name; }

		virtual ~Argument() = default;
	};

	/**
	 * The Argument and CodeArgument are represented by two different classes
	 * because they have different serialization logic.
	 */
	class TextArgument final: public Argument {
		std::string content;

	public:
		TextArgument(std::string name, std::string content):
			  Argument(std::move(name)),
			  content(std::move(content)) {}

		Box<dia_args::Component> getValue(MessageBase&) override;
	};

	class CodeArgument final: public Argument {
		dia::SourcePosition position;
		usize               lines_before = 1;
		usize               lines_after  = 1;

	public:
		static void addCodeLines(
			std::vector<Box<dia_args::Component>>& code_list,
			Ref<tokenizer::TokenSource>            source,
			usize                                  start,
			usize                                  end
		);

		Box<dia_args::Component> getValue(MessageBase&) override;

		CodeArgument(std::string name, dia::SourcePosition position):
			  Argument(std::move(name)),
			  position(position) {}
	};

	class CodeLocationArgument final: public Argument {
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

		Box<dia_args::Component> getValue(MessageBase&) override;
	};

	/**
	 * @brief InteractiveElement is an interface for elements that can be used
	 * as arguments in InteractiveArgument.
	 * They can add new messages and entities when generating their value.
	 */
	class InteractiveElement {
	public:
		virtual Box<dia_args::Component> getValue(MessageBase&) = 0;

		virtual ~InteractiveElement() = default;
	};

	class InteractiveArgument final: public Argument {
	private:
		Box<InteractiveElement> element;

	public:
		InteractiveArgument(std::string name, Box<InteractiveElement> element):
			  Argument(std::move(name)),
			  element(std::move(element)) {}

		Box<dia_args::Component> getValue(MessageBase& message) override {
			return element->getValue(message);
		}
	};

	/**
	 * @brief Entities are used to represent symbols in the error code messages.
	 * Each entity has its own links.
	 *
	 * For example a variable can be represented as an entity
	 * and in every code snippet every time the variable appears in the text
	 * it will be linked to the same entity.
	 */
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

	/**
	 * @brief Text based entity is an entity that is represented by a simple text
	 * For example a variable with variable name,
	 * every time the variable name appears in the code snippet
	 * it will be linked to the same entity.
	 * @warning This is not implemented yet
	 */
	class TextBasedEntity final: public Entity {
		std::string displayed_name;

	public:
		TextBasedEntity(std::vector<std::string> linked_messages, std::string displayed_name):
			  Entity(std::move(linked_messages)),
			  displayed_name(std::move(displayed_name)) {}

		[[nodiscard]] const std::string& getDisplayedName() const { return displayed_name; }
	};

	/**
	 * @brief Pointer message is an information for where the pointer should point
	 * in the code snippet. The content of the pointer message is defined in the message template
	 * with the right ID and here we just specify the ID.
	 */
	class PointerMessage final {
	public:
		dia::SourcePosition         position;
		std::string                 pointer_message_id;
		base::Optional<std::string> message_id;

		PointerMessage(
			std::string                 name,
			dia::SourcePosition         position,
			base::Optional<std::string> message_id = {}
		):
			  position(position),
			  pointer_message_id(std::move(name)),
			  message_id(std::move(message_id)) {}
	};

	class ExploreLink final {
		std::string                message_id;
		std::vector<Box<Argument>> arguments;

	public:
		ExploreLink(std::string message_id, std::vector<Box<Argument>> arguments):
			  message_id(std::move(message_id)),
			  arguments(std::move(arguments)) {}

		dia_args::ExploreLink getValue(MessageBase& message) const;
	};

	class MessageBase {
	private:
		std::vector<Box<Argument>> arguments;

		std::vector<Box<Entity>> entities;

		/**
		 */
		std::vector<PointerMessage> pointer_messages;
		std::vector<ExploreLink>    explore_links;

		/**
		 * @brief Messages that are directly attached to the this message and will be displayed
		 * below it.
		 */
		std::vector<Box<MessageBase>> attached_messages;

		/**
		 * @brief Additional messages that are not attached directly to this diagnostic,
		 * but may be the link destination or explore link target.
		 */
		base::HashMap<std::string, Box<MessageBase>> linked_messages;

		virtual Metadata getMetadata() const = 0;


		dia_args::Message buildMessages(
			base::HashMap<std::string, dia_args::Message>& additional_messages
		);

	protected:
		MessageBase() = default;

	public:
		static std::string getUniqueID();

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

		/**
		 * @brief Adds a pointer message to this message.
		 */
		void addPointerMessage(PointerMessage msg) { pointer_messages.push_back(std::move(msg)); }

		template<typename... Args>
		void addPointerMessage(Args&&... args) {
			pointer_messages.push_back(PointerMessage(std::forward<Args>(args)...));
		}

		/**
		 * @brief Adds an entity to this message.
		 */
		void addEntity(Box<Entity> entity) { entities.push_back(std::move(entity)); }

		template<typename T, typename... Args>
		void addEntity(Args&&... args) {
			entities.push_back(base::makeBox<T>(std::forward<Args>(args)...));
		}

		/**
		 * @brief Adds an explore link.
		 */
		void addExploreLink(ExploreLink link) { explore_links.push_back(std::move(link)); }

		template<typename... Args>
		void addExploreLink(Args&&... args) {
			explore_links.push_back(ExploreLink(std::forward<Args>(args)...));
		}

		/* =============================  ACCESSORS ============================= */

		/**
		 * @note These method is used by the CodeParameter to get the pointer messages on the code.
		 */
		const std::vector<PointerMessage>& getPointerMessages() const { return pointer_messages; }

		// ============================== ADDING MESSAGES ==============================

		/**
		 * @note The methods below can be used directly by the users
		 */

		/**
		 * @brief Adds a linked message that can be linked but is not attached.
		 * It is not displayed, but can be linked from the other messages.
		 * The pointer messages can also be linked here.
		 */
		void addLinkedMessage(std::string id, Box<MessageBase> message) {
			linked_messages.insertOrAssign(std::move(id), std::move(message));
		}

		/**
		 * @brief Attachs a message that will be displayed below this message.
		 */
		void attachMessage(Box<MessageBase> note) { attached_messages.push_back(std::move(note)); }

		// ============================  BUILDING DIAGNOSTIC FILE =============================

		/**
		 * @brief Main method that builds the diagnostic file representation of this diagnostic.
		 */
		Box<dia_args::Diagnostic> buildDiagnosticFile();


		virtual ~MessageBase() = default;
	};

}
