/**
 * @file exec_builtin.hpp
 * @brief executed builtin operations on CTVs
 */

#pragma once

#include "exec.hpp"

namespace exec {
	using Args = const std::vector<exec::CTV>&;

	template<typename T>
	exec::CTV compareBuiltin(Args ctvs) {
		T val_a = ctvs[0].getData<T>().front();
		T val_b = ctvs[1].getData<T>().front();

		int8_t res = 0;
		if (val_a < val_b) res = 1;
		if (val_a > val_b) res = -1;

		exec::CTV ctvResult = exec::alloc_new(ts::TypeDesc<>(ts::IntegralInfo::create(8)), 8);
		ctvResult.getData<int8_t>().front() = res;
		return ctvResult;
	}

	template<typename T>
	exec::CTV equalityBuiltin(Args ctvs) {
		T val_a = ctvs[0].getData<T>().front();
		T val_b = ctvs[1].getData<T>().front();

		int8_t res = (val_a == val_b);

		exec::CTV ctvResult = exec::alloc_new(ts::TypeDesc<>(ts::BoolInfo::create()), 8);
		ctvResult.getData<bool>().front() = res;
		return ctvResult;
	}

	template<typename T>
	exec::CTV assignBuiltin(Args ctvs) {
		ctvs[0].getData<T>().front() = ctvs[1].getData<T>().front();
		return ctvs[0];
	}

	template<typename T>
	exec::CTV emptyBuiltin(Args ctvs) {
		ctvs[0].getData<T>().front() = 0;
		return ctvs[0];
	}
}  // namespace exec
