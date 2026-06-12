#include <abi/layout/target.hpp>
#include <tsl/c_abi_target.hpp>

namespace compiler::tsl {

	const abi::layout::TargetABI& compilerTargetABI() { return abi::layout::hostTargetABI(); }

}
