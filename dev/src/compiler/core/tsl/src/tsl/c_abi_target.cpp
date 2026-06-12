#include <tsl/c_abi_target.hpp>

#include <abi/layout/target.hpp>

namespace compiler::tsl {

	const abi::layout::TargetABI& compilerTargetABI() { return abi::layout::hostTargetABI(); }

}
