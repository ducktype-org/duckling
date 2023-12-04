/**
 * @file source_position.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include <printer/printer.hpp>
#include <string>

/** 
	* @brief  Type used for storing token position in a source file
	* 
	* It always stores a valid position with the position (end() + 1, end() + 1) being EOF
	*/
class SourcePosition {
public:
	using Source = std::shared_ptr<const fs::FilePath>;

	SourcePosition() = delete;

	SourcePosition(Source source_code, usize line, usize column, usize start);
	SourcePosition(
		Source source_code, usize line, usize column, usize start, usize end
	);
	SourcePosition(const SourcePosition& other);
	SourcePosition(const SourcePosition& other, usize end);

	/**
		* @brief Get a copy of the bytes in this position
		* 
		* @return std::string containing a copy of the bytes in this position, if it fails it returns error message instead
		*/
	[[nodiscard]]
	std::string getSourceChars() const;
	/**
		* @brief generates formatted error message with a given reason
		* 
		* @param reason contains the reason for the error
		*/
	printer::Message genErrorMsg(std::string_view reason) const;
	/**
		* @brief generates formatted error string with a given reason
		* 
		* @param reason contains the reason for the error
		*/
	std::string genErrorStr(std::string_view reason) const;

	usize getLine() const;
	usize getColumn() const;
	usize getStart() const;
	usize getEnd() const;
	Source getSource() const;

private:
	usize line, column; ///< #line, #column describe start position in code for the user
	usize source_start, source_end;  ///< #source_start, #source_end describe range of bytes in the file
	Source source_code; ///< pointer to source file data
};
