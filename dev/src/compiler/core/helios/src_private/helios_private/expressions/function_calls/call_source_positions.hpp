#pragma once

#include <diagnostic/source_position.hpp>

#include <vector>

/**
 * @brief Holds the source positions related to a call expression, used for diagnostics and for
 * creating ElementOrigin for the processed HOUT calls.
 */
struct CallSourcePositions {
	/*
	 * @brief The source position of the entire callee.
	 * E.g. the function identifier or the operator symbol.
	 */
	dia::SourcePosition callee;

	/**
	 * @brief The source position of the part of the call that includes all the arguments. For a
	 * function call, this would be the parentheses and everything in between. For a binary
	 * operator, this would be from the lhs argument to the rhs argument.
	 */
	dia::SourcePosition arg_group;

	/**
	 * @brief The source positions of the individual arguments.
	 */
	std::vector<dia::SourcePosition> args;
};
