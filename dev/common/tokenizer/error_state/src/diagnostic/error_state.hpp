/**
 * @file error_state.hpp
 * @author Andrzej
 */

#pragma once

#include <diagnostic/source_position.hpp>
#include <printer/printer.hpp>

namespace dia {
	/**
	 * @brief Class used to store whether there was no error and errors for later output
	 * 
	 * @note We will later change to storing errors in a more structured way, not just a string
	 */
	class ErrorState {
		bool  failbit   = false; ///< Whether an error was encountered
		usize err_count = 0; ///< Number of encountered errors
		printer::Console errorLog; ///< Error/Warning Log

		/**
		 * @brief Note that an error was encountered
		 */
		void setFail() { failbit = true; }

	public:
		ErrorState()                        = default;
		ErrorState(ErrorState&&)            = default;
		ErrorState& operator=(ErrorState&&) = default;

		/**
		 * @brief Add a positional error
		 * 
		 * @param position position of the token from which the error originated
		 * @param message custom message to include in the error
		 */
		void failAndLog(const SourcePosition& position, std::string_view message);
		/**
		 * @brief Add a non-fatal positional error
		 * 
		 * @param position position of the token from which the error originated
		 * @param message custom message to include in the error
		 */
		void logError(const SourcePosition& position, std::string_view message);
		/**
		 * @brief Add an error
		 * 
		 * @param message custom message to include in the error
		 */
		void failAndLog(printer::Message message);
		/**
		 * @brief Add a non-fatal error
		 * 
		 * @param message custom message to include in the error
		 */
		void logError(printer::Message message);
		/**
		 * @brief Print current errors to output
		 * 
		 * @param stream output stream
		 */
		void dumpLog(std::ostream& stream = std::cerr) const;

		/**
		 * @return true If no errors encountered
		 * @return false If errors encountered
		 */
		[[nodiscard]]
		bool good() const {
			return !failbit;
		}

		/**
		 * @return true If errors encountered
		 * @return false If no errors encountered
		 */
		[[nodiscard]]
		bool fail() const {
			return failbit;
		}

		/**
		 * @return usize Number of errors encountered
		 */
		[[nodiscard]]
		usize errCount() const;
	};
}
