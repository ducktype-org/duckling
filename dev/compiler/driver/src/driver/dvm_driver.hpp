#pragma once

#include <base/box.hpp>
#include <base/ref.hpp>

#include "driver.hpp"

namespace compiler::driver {

	class DVMDriver final: public BackendDriver {
	public:
		DVMDriver(CRef<Options> options): BackendDriver(options) {}

		void compileModule(query::Context&, const BackendModuleData&) override {
			throw base::NotYetImplemented("compilation for BC driver");
		}

		void link() final {
			// DVM doesn't require linking.
		}
	};
}
