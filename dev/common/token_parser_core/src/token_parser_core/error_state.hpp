/**
 * @file error_state.hpp
 * @author Andrzej
 */

#pragma once

#include <printer/printer.hpp>
#include <lexer/token.hpp>

namespace tpc {
	class ErrorState {
		bool  failbit   = false;
		usize err_count = 0;
		// error list
		// @TODO: add info about file for printing
		printer::Console errorLog;

	public:
		ErrorState()                        = default;
		ErrorState(ErrorState&&)            = default;
		ErrorState& operator=(ErrorState&&) = default;

		void setFail() { failbit = true; }

		void failAndLog(const lexer::SourcePosition& position, std::string_view message);
		void logError(const lexer::SourcePosition& position, std::string_view message);
		void dumpLog(std::ostream& stream = std::cerr) const;

		[[nodiscard]]
		bool good() const {
			return !failbit;
		}

		[[nodiscard]]
		bool fail() const {
			return failbit;
		}

		[[nodiscard]]
		usize errCount() const;
	};
}
