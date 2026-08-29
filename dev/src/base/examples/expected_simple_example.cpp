#include <base/collections/expected.hpp>

#include <cassert>
#include <expected>
#include <string>
#include <string_view>

namespace {
	std::expected<int, std::string> parsePort(std::string_view text) {
		if (text == "8080") return 8'080;
		return std::unexpected("not a port: " + std::string(text));
	}

	std::expected<void, std::string> save(bool works) {
		if (works) return {};
		return std::unexpected<std::string>("disk is full");
	}
}

int main() {
	// Matching on a value, and on an error.
	match_expected(parsePort("8080")) {
		exp_ok(port) { assert(8'080 == port); }
		exp_err(error [[maybe_unused]]) { assert(false && "8080 is a port"); }
	}

	match_expected(parsePort("duck")) {
		exp_ok(port [[maybe_unused]]) { assert(false && "duck is not a port"); }
		exp_err(error) { assert("not a port: duck" == error); }
	}

	// --------------------------------------------------

	// std::expected<void, E> has nothing to bind, so the parentheses stay empty.
	bool saved = false;
	match_expected(save(true)) {
		exp_ok() { saved = true; }
		exp_err() { assert(false && "saving works here"); }
	}
	assert(saved);

	// --------------------------------------------------

	// One branch only? Use the shorthands.
	auto port = parsePort("8080");
	if_exp_ok(port, value) { assert(8'080 == value); }

	if_exp_err(save(false), error) { assert("disk is full" == error); }

	// The error can be moved out of the expected as well.
	match_expected(save(false)) {
		exp_ok() { assert(false && "saving fails here"); }
		exp_err_move(error) { assert("disk is full" == std::string(std::move(error))); }
	}
}
