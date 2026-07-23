#include <abi/target.hpp>
#include <tsl/c_abi_target.hpp>

namespace compiler::tsl {

	const abi::TargetABI& compilerTargetABI() { return abi::hostTargetABI(); }
}
