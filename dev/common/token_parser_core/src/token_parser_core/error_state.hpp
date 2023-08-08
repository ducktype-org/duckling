/**
 * @file error_state.hpp
 * @author Andrzej
 */

#pragma once

#include <printer/printer.hpp>
#include <lexer/token.hpp>

namespace tpc {
	class ErrorState {
		bool failbit = false;
		usize err_count = 0;
		// error list
		// @TODO: add info about file for printing
		printer::Console errorLog;
	public:
		ErrorState() = default;
		ErrorState(ErrorState&&) = default;
		ErrorState& operator=(ErrorState&&) = default;
		void setFail() {
			failbit = true;
		}
		void failAndLog(lexer::Token::Position position, std::string message);
		void logError(lexer::Token::Position position, std::string message);
		void dumpLog(std::ostream& stream = std::cerr) const;
		bool good() const {
			return !failbit;
		}
		bool fail() const {
			return failbit;
		}

		usize errCount() const;
	};
}