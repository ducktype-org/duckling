/**
 * @file logger.hpp
 * @author Maurycy Wojda
 */

#pragma once

#include <diagnostic/source_position.hpp>
#include <printer/stream_printer.hpp>

#include "message.hpp"
#include "diagnostic_converters.hpp"

namespace dia {
	/**
	 * @brief Class used to log diagnostic messages for later output.
	 */
	class Logger {
		/**
		 * @brief Storage for messages of each severity.
		 */
		// @TODO: We may want to store a priority queue sorted by Message::Domain.
		std::array<std::vector<base::unique_ptr<Message>>, Message::NUM_SEVERITIES> message_log
			= {};

	public:
		Logger()                    = default;
		Logger(Logger&&)            = default;
		Logger& operator=(Logger&&) = default;

		/**
		 * @brief Whether or not to immediately dump a logged message to std::cerr by default.
		 *
		 * Immediately dumping logged messages may be useful when debugging.
		 */
		static constexpr bool IMMEDIATELY_DUMP =
#ifdef PRINT_LOG
			true
#else
			false
#endif
			;


		/**
		 * @brief Log a message.
		 *
		 * @param message_ptr A base::unique_ptr to the Message to be logged.
		 * @param detailed Whether to dump detailed logs if immediately dumping.
		 * @param immediately_dump Whether to immediately dump the log to std::cerr.
		 */
		void
			log(base::unique_ptr<Message> message_ptr,
		        bool                      detailed         = true,
		        bool                      immediately_dump = IMMEDIATELY_DUMP);

		/**
		 * @brief Print all logged messages to a stream.
		 *
		 * @tparam Converter The DiagnosticToStringConverter instance to use when dumping.
		 * @param detailed Whether to print detailed messages.
		 * @param stream The stream to print to.
		 */
		template<DiagnosticToPrinterConverter Converter = DiagnosticToUserConverter>
		void dumpLog(bool detailed, std::ostream& stream = std::cerr) const;

		/**
		 * @return true If no errors were encountered.
		 * @return false If errors were encountered.
		 */
		[[nodiscard]]
		bool good() const {
			return messageCount(Message::Severity::Error) == 0;
		}

		/**
		 * @return true If errors were encountered.
		 * @return false If no errors were encountered.
		 */
		[[nodiscard]]
		bool bad() const {
			return !good();
		}

		/**
		 * @param s The severity to be counted
		 * @return usize Number of messages of given severity encountered.
		 */
		[[nodiscard]]
		usize messageCount(Message::Severity s) const;

		/**
		 * @return usize Number of messages encountered.
		 */
		[[nodiscard]]
		usize messageCount() const;

		// Behold, for what you see ahead is the land of the obsolete!

		/**
		 * @brief Add a positional error.
		 *
		 * @param position position of the token from which the error originated
		 * @param message custom message to include in the error
		 *
		 * @deprecated Use custom Message subclasses instead.
		 * @todo remove
		 */
		void failAndLog(const SourcePosition& position, std::string_view message);

		/**
		 * @brief Add an error.
		 *
		 * @param message custom message to include in the error
		 *
		 * @deprecated Use custom Message subclasses instead.
		 * @todo remove
		 */
		void failAndLog(const std::string& message);

		/**
		 * @brief Clear all logs. Restore to default state.
		 */
		void clear();

		/**
		 * @brief Print all logged messages to a stream. Then, clear all logs.
		 *
		 * Equivalent to calling dumpLog() and clear().
		 *
		 * @param detailed Whether to print detailed messages.
		 * @param stream The stream to print to.
		 */
		void dumpLogAndClear(bool detailed, std::ostream& stream);
	};
}
