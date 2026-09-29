#pragma once

#include <replxx.hxx>
using Replxx = replxx::Replxx;

namespace vm::debugger::cli::impl {
	class ImplementationSpecific {
	public:
		Replxx replxx;
	};
}

#include <vm/debugger/UI/CLI/cli.hpp>
