/**
 * @file create_default.hpp
 * @brief Creates defaultable operations (but generally does not add them to operations)
 */
#pragma once

#include <functional>
#include <operations/operation.hpp>
#include <typesystem/typesystem.hpp>

namespace operation {
	TypedOperation createDefaultEquality(ts::TypeInfo type_info);
	TypedOperation createDefaultComparison(ts::TypeInfo type_info);
	TypedOperation createDefaultAssign(ts::TypeInfo type_info);
	TypedOperation createDefaultConstructEmpty(ts::TypeInfo type_info);
	TypedOperation createDefaultConstructFull(ts::TypeInfo type_info);

	// This only compares virtual ancestors.
	// Result of this and the one created by createDefaultEquality complement each other.
	// They should be stored separately and called in specific manner.
	// TypedOperation createDefaultVirtualEquality(ts::ClassInfo class_info);

	inline TypedOperation createDefault(ts::TypeInfo type_info, Defaultable kind) {
		switch (kind) {
			using enum Defaultable;
		case Equality:
			return createDefaultEquality(type_info);
		case Compare:
			return createDefaultComparison(type_info);
		case Assign:
			return createDefaultAssign(type_info);
		case ConstructEmpty:
			return createDefaultConstructEmpty(type_info);
		case ConstructFull:
			return createDefaultConstructFull(type_info);
		default:
			RIFT_PANIC("Illegal enum value.");
		}
	}

	inline TypedOperation createAddDefault(ts::TypeInfo type_info, Defaultable kind) {
		auto to = createDefault(type_info, kind);
		addDefault(kind, type_info, to);
		return to;
	}

	inline TypedOperation comparison(const Operation& op, ts::TypeDesc<> ty) {
		return {
			op,
			ts::FunctionInfo::create({ ty, ty }, query::entryPoint<ts::QueryIntegralType>({ 8 }))
		};
	}

	inline TypedOperation equality(const Operation& op, ts::TypeDesc<> ty) {
		return { op,
			     ts::FunctionInfo::create({ ty, ty }, query::entryPoint<ts::QueryBoolType>({})) };
	}

	inline TypedOperation assign(const Operation& op, ts::TypeDesc<> ty) {
		return { op, ts::FunctionInfo::create({ ty, ty }, ty) };
	}

	inline TypedOperation construct(const Operation& op, ts::TypeDesc<> ty) {
		return { op, ts::FunctionInfo::create({ ty }, ty) };
	}
}
