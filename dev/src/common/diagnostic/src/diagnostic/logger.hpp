/**
 * @file logger.hpp
 * @author Maurycy Wojda
 * @brief Module implementing logging for source code
 * (with location, error structure, etc.).
 *
 * ### Usage:
 * @include logger_example.cpp
 *
 * Interface
 * =========
 *
 * All symbols are in namespace ``dia``.
 *
 * Error State
 * -----------
 *
 * Logger is a class that keeps track of errors, warnings, and other messages and stores them for
 * future output.
 *
 * ### Adding errors
 *
 * New errors are added using the ``log()`` method.
 * Every message must extend the ``Error``, `` Warning``,``Info``, or ``Hint`` class and implement
 * the ``getDomain()`` and ``toStringBrief()`` methods. The first one treats it as a fatal error
 * while the second treats it more like a warning. There are two versions for both. One takes
 * position and message while the other takes only message.
 *
 * ### Printing errors
 *
 * Errors are printed to output using the ``dumpLog()`` method.
 *
 * ### Checking state
 *
 * Methods ``good()``, ``bad()`` and ``messageCount()`` are used to get information about the
 * current number of messages. The ``messageCount()`` method can take a severity as an argument and
 * thus can be used to count errors and warnings.
 *
 * @example logger_example.cpp
 */

#pragma once

#include "diagnostic_converters.hpp"
#include "message.hpp"

#include <base/pointers/box.hpp>

#include <diagnostic/source_position.hpp>
#include <printer/stream_printer.hpp>

#include <iostream>

namespace dia {
	/**
	 * @brief Class used to log diagnostic messages for later output.
	 */
	class Logger final {
		/**
		 * @brief Storage for messages of each severity.
		 */
		// @TODO: We may want to store a priority queue sorted by Message::Domain.
		std::array<std::vector<Box<Message>>, Message::NUM_SEVERITIES> message_log = {};

	public:
		Logger()                    = default;
		Logger(Logger&&)            = default;
		Logger& operator=(Logger&&) = default;


		/**
		 * @brief Log a message.
		 *
		 * @param message_ptr A Box to the Message to be logged.
		 */
		void log(Box<Message> message_ptr);

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
