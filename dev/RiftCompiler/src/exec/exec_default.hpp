/**
 * @file exec_default.hpp
 * @brief actual implementation of default operation, lambdas in operations use functions from here
 */

#pragma once

#include "exec.hpp"
#include <operations/operation.hpp>

namespace exec {
	CTV defaultEquality(
		ts::TypeInfo type_info, const operation::Calls& calls, const CTV& a, const CTV& b
	);

	CTV defaultEqualityVirtual(ts::ClassInfo class_info, const CTV& a, const CTV& b);

	CTV defaultCompare(
		ts::TypeInfo type_info, const operation::Calls& calls, const CTV& a, const CTV& b
	);

	CTV defaultAssign(
		ts::TypeInfo type_info, const operation::Calls& calls, const CTV& a, const CTV& b
	);

	CTV defaultConstructEmpty(
		ts::TypeInfo type_info, const operation::Calls& calls, const CTV& ctv
	);

	CTV defaultConstructFull(
		ts::TypeInfo type_info, const operation::Calls& calls, const std::vector<CTV>& ctvs
	);
}
