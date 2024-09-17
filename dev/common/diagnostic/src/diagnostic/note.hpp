#pragma once

#include "source_position.hpp"

namespace dia {
	/**
	 * @brief A supplementary piece of information aimed to enhance a dia::Message.
	 *
	 * Examples of a Note include "note: previous declaration here" in a redeclaration message.
	 *
	 * Each Note can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * Each Note has two (not necessarily different) printable messages. One brief,
	 * and one detailed. The latter may contain extra information about the source
	 * or nature of the message and may be used to explain the message to beginners.
	 */
	class Note {
	protected:
		/**
		 * @brief Convert the Note to an std::string containing the diagnostic minimum.
		 * @return The brief std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual std::string toStringBrief() const
			= 0;

		/**
		 * @brief Convert the Note to an std::string which possibly contains information
		 * not included in the diagnostic minimum, thus not included in the brief message.
		 * @return The detailed std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual std::string toStringDetailed() const {
			// By default, the detailed version is the same as the brief version.
			return toStringBrief();
		}

	public:
		/**
		 * @brief Get the SourcePosition relevant to this Note, if it exists.
		 * @return The SourcePosition relevant to this Note.
		 */
		[[nodiscard]]
		virtual base::Optional<SourcePosition> getSourcePosition() const {
			return {};
		}

		/**
		 * @brief Get an std::string ready to be printed for the user to view.
		 *
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the message and
		 * may be used to explain the message to beginners.
		 * @return An std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		std::string toString(bool detailed) const {
			return detailed ? toStringBrief() : toStringDetailed();
		}

		virtual ~Note() noexcept = default;
	};

	class NoteWithPosition: public Note {
		/**
		 * @brief The source position relevant to this note, e.g. the position of an original
		 * declaration.
		 */
		SourcePosition source_position;

	protected:
		/**
		 * @brief Construct a NoteWithPosition from a relevant SourcePosition.
		 * @param source_position The SourcePosition relevant to this Note.
		 */
		explicit NoteWithPosition(const SourcePosition& source_position):
			  source_position(source_position) {}

	public:
		/**
		 * @copydoc Note::getSourcePosition
		 */
		[[nodiscard]]
		base::Optional<SourcePosition> getSourcePosition() const override {
			return { source_position };
		}
	};
}
