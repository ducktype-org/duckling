/**
 * @file operatorutils.hpp
 * @brief macros implementing builtin operations on ints, and adding them to operations
 */

#pragma once

#include <exec/ctv.hpp>
#include <exec/operators/builtinoperators.hpp>
#include <operations/operation.hpp>

#include <base/exceptions.hpp>

#include <vector>

namespace exec::operators {
	// Specializations can be added for types that cannot be simply casted this way.
	// @TODO: this function should be exported in some form by type system
	template<class T>
	T CTVToType(const CTV ctv) {
		return *ctv.getData<T>().data();
	}

	// Specializations can be added for types that cannot be simply casted this way.
	// @TODO: this function should be exported in some form by type system
	template<class T>
	void PutDataInCTV(const CTV ctv, T value) {
		*ctv.getData<T>().data() = value;
	}
}

#define BIN_SIMPLE_OP(ttype, operation, name)                                                    \
	CTV name(std::vector<CTV> ctvs) {                                                            \
		CTV res = alloc_new(ts::TypeDesc<>(ctvs[0].type.getType()));                             \
		PutDataInCTV<ttype>(res, CTVToType<ttype>(ctvs[0]) operation CTVToType<ttype>(ctvs[1])); \
		return res;                                                                              \
	}

// For functions of simple binary operations on ints.
#define INT_BIN_SIMPLE_OP(size, operation, name) \
	BIN_SIMPLE_OP(i##size, operation, bin_##name##_int_##size)

#define INT_BIN_SIMPLE_OPS(operation, name) \
	INT_BIN_SIMPLE_OP(8, operation, name)   \
	INT_BIN_SIMPLE_OP(16, operation, name)  \
	INT_BIN_SIMPLE_OP(32, operation, name)  \
	INT_BIN_SIMPLE_OP(64, operation, name)  \
	INT_BIN_SIMPLE_OP(128, operation, name)

// For functions of simple binary operations on uints.
#define UINT_BIN_SIMPLE_OP(size, operation, name) \
	BIN_SIMPLE_OP(u##size, operation, bin_##name##_uint_##size)

#define UINT_BIN_SIMPLE_OPS(operation, name) \
	UINT_BIN_SIMPLE_OP(8, operation, name)   \
	UINT_BIN_SIMPLE_OP(16, operation, name)  \
	UINT_BIN_SIMPLE_OP(32, operation, name)  \
	UINT_BIN_SIMPLE_OP(64, operation, name)  \
	UINT_BIN_SIMPLE_OP(128, operation, name)

// For aggregated functions on all numerical types.
#define NUM_BIN_SIMPLE_OPS(operation, name) \
	INT_BIN_SIMPLE_OPS(operation, name)     \
	UINT_BIN_SIMPLE_OPS(operation, name)

// For built in operator map initialization of binary operators.
#define BIN_ENTRY(arg_info_1, arg_info_2, res_info, op_name, fun_name)                            \
	operation::TypedOperation typed_op                                                            \
		= { fun_name,                                                                             \
		    query::entryPoint<ts::QueryFunctionType>({ { arg_info_1, arg_info_2 }, res_info }) }; \
	operation::OperationId id = operation::addOperation(typed_op);                                \
	getBuiltInOps().put({ Operator::op_name, { arg_info_1, arg_info_2 } }, id);

#define BIN_ENTRY_SIMPLE(info, op_name, fun_name) BIN_ENTRY(info, info, info, op_name, fun_name)

#define BIN_ENTRY_SIMPLE_INFO(info, op_name, fun_name) \
	{ BIN_ENTRY_SIMPLE(info, op_name, operators::fun_name) }

// For initializing simple binary operations on ints.
#define INT_BIN_ENTRY_SIMPLE(size, op_name)                              \
	{                                                                    \
		auto info = query::entryPoint<ts::QueryIntegralType>({ size });  \
		BIN_ENTRY_SIMPLE_INFO(info, op_name, bin_##op_name##_int_##size) \
	}

#define INT_BIN_ENTRIES_SIMPLE(op_name) \
	INT_BIN_ENTRY_SIMPLE(8, op_name)    \
	INT_BIN_ENTRY_SIMPLE(16, op_name)   \
	INT_BIN_ENTRY_SIMPLE(32, op_name)   \
	INT_BIN_ENTRY_SIMPLE(64, op_name)   \
	INT_BIN_ENTRY_SIMPLE(128, op_name)

// For initializing simple binary operations on uints.
#define UINT_BIN_ENTRY_SIMPLE(size, op_name)                                   \
	{                                                                          \
		auto info = query::entryPoint<ts::QueryIntegralType>({ size, false }); \
		BIN_ENTRY_SIMPLE_INFO(info, op_name, bin_##op_name##_uint_##size)      \
	}

#define UINT_BIN_ENTRIES_SIMPLE(op_name) \
	UINT_BIN_ENTRY_SIMPLE(8, op_name)    \
	UINT_BIN_ENTRY_SIMPLE(16, op_name)   \
	UINT_BIN_ENTRY_SIMPLE(32, op_name)   \
	UINT_BIN_ENTRY_SIMPLE(64, op_name)   \
	UINT_BIN_ENTRY_SIMPLE(128, op_name)

// For aggregated initialization of all numerical map entries.
#define NUM_BIN_ENTRIES_SIMPLE(op_name) \
	INT_BIN_ENTRIES_SIMPLE(op_name)     \
	UINT_BIN_ENTRIES_SIMPLE(op_name)
