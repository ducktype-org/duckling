/**
 * @file expected.hpp
 * @brief Pattern matching macros for ``std::expected``, the analogue of ``match_optional``.
 *
 * ### Usage:
 * @code
    std::expected<int, std::string> result = 4;
    match_expected(result) {
        exp_ok(value) { std::cout << "Value: " << value << '\n'; }
        exp_err(error) { std::cout << "Error: " << error << '\n'; }
    }

    // std::expected<void, E> holds no value, so simply leave the parentheses empty.
    std::expected<void, std::string> saved = save();
    match_expected(saved) {
        exp_ok() { std::cout << "Saved!\n"; }
        exp_err(error) { std::cout << "Error: " << error << '\n'; }
    }

    // Or simply
    if_exp_ok(result, value) { std::cout << "Value: " << value << '\n'; }
    if_exp_err(result, error) { std::cout << "Error: " << error << '\n'; }
 * @endcode
 *
 * Output:
 * @code
    Value: 4
    Saved!
    Value: 4
 * @endcode
 *
 * @example expected_simple_example.cpp
 */
#pragma once

#include <base/preproc/diagnostics.hpp>

#include <expected>
#include <utility>

/* Some cool macros.
 *
 * They mirror match_optional and friends, but they read the way std::expected does: a value is
 * "ok", everything else is an "err".
 *
 * Example use:
 *
 * std::expected<int, std::string> result = 4;
 * match_expected(result) {
 * 	 exp_ok(value) {
 * 	   std::cout << "Value: " << value << '\n';
 * 	 }
 * 	 exp_err(error) {
 * 	   std::cout << "Error: " << error << '\n';
 * 	 }
 * }
 *
 * // The name is optional in both branches, so std::expected<void, E> works too.
 * std::expected<void, std::string> saved = save();
 * match_expected(saved) {
 * 	 exp_ok() {
 * 	   std::cout << "Saved!\n";
 * 	 }
 * 	 exp_err() {
 * 	   std::cout << "Something went wrong.\n";
 * 	 }
 * }
 *
 * // Use the _move flavours to take the value or the error out of the expected.
 * match_expected(std::move(result)) {
 * 	 exp_ok_move(value) { sink(std::move(value)); }
 * 	 exp_err_move(error) { return std::unexpected(std::move(error)); }
 * }
 *
 * // Or simply
 * if_exp_ok(result, value) {
 * 	std::cout << "Value: " << value << '\n';
 * }
 *
 * if_exp_err(result, error) {
 * 	std::cout << "Error: " << error << '\n';
 * }
 *
 */
#define match_expected(expected) \
	PUSH_DIAGNOSTIC              \
	NO_SHADOW                    \
	if (auto&& _internal_expected = (expected); true) POP_DIAGNOSTIC

#define exp_ok(...)                     \
	PUSH_DIAGNOSTIC                     \
	NO_SHADOW                           \
	if (_internal_expected.has_value()) \
	__VA_OPT__(if (auto&& __VA_ARGS__ = *_internal_expected; true)) POP_DIAGNOSTIC

#define exp_ok_move(_value_name)        \
	PUSH_DIAGNOSTIC                     \
	NO_SHADOW                           \
	if (_internal_expected.has_value()) \
		if (auto&& _value_name = *std::move(_internal_expected); true) POP_DIAGNOSTIC

#define exp_err(...)                     \
	PUSH_DIAGNOSTIC                      \
	NO_SHADOW                            \
	if (!_internal_expected.has_value()) \
	__VA_OPT__(if (auto&& __VA_ARGS__ = _internal_expected.error(); true)) POP_DIAGNOSTIC

#define exp_err_move(_err_name)          \
	PUSH_DIAGNOSTIC                      \
	NO_SHADOW                            \
	if (!_internal_expected.has_value()) \
		if (auto&& _err_name = std::move(_internal_expected).error(); true) POP_DIAGNOSTIC

#define if_exp_ok(expected, ...)                                                \
	PUSH_DIAGNOSTIC                                                             \
	NO_SHADOW                                                                   \
	if (auto&& _internal_expected = (expected); _internal_expected.has_value()) \
	__VA_OPT__(if (auto&& __VA_ARGS__ = *_internal_expected; true)) POP_DIAGNOSTIC

#define if_exp_err(expected, ...)                                                \
	PUSH_DIAGNOSTIC                                                              \
	NO_SHADOW                                                                    \
	if (auto&& _internal_expected = (expected); !_internal_expected.has_value()) \
	__VA_OPT__(if (auto&& __VA_ARGS__ = _internal_expected.error(); true)) POP_DIAGNOSTIC
