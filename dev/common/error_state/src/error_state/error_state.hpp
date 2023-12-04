/**
 * @file error_state.hpp
 * @author Andrzej
 */

#pragma once

#include <source_position/source_position.hpp>
#include <printer/printer.hpp>

class ErrorState {
	bool  failbit   = false;
	usize err_count = 0;
	// error list
	printer::Console errorLog;

public:
	ErrorState()                        = default;
	ErrorState(ErrorState&&)            = default;
	ErrorState& operator=(ErrorState&&) = default;

	void setFail() { failbit = true; }

	void failAndLog(const SourcePosition& position, std::string_view message);
	void logError(const SourcePosition& position, std::string_view message);
	void failAndLog(printer::Message message);
	void logError(printer::Message message);
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
