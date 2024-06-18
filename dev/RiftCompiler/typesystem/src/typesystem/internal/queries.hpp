#pragma once

#include <query_framework/query_int.hpp>

#include "../type_interface.hpp"

namespace ts::internal {
	class TupleInfoImpl;

	struct KeyFor_QuerySizeOfTuple {
		const ts::internal::TupleInfoImpl* value;
		KeyFor_QuerySizeOfTuple() = delete;
		KeyFor_QuerySizeOfTuple(const ts::internal::TupleInfoImpl* value): value(value) {}
		auto operator<=>(const KeyFor_QuerySizeOfTuple& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const { return base::HashT(value); }
	};

	DECLARE_QUERY(QuerySizeOfTuple, KeyFor_QuerySizeOfTuple, usize)

	class VariantInfoImpl;

	struct KeyFor_QuerySizeOfVariant {
		const ts::internal::VariantInfoImpl* value;
		KeyFor_QuerySizeOfVariant() = delete;
		KeyFor_QuerySizeOfVariant(const ts::internal::VariantInfoImpl* value): value(value) {}
		auto operator<=>(const KeyFor_QuerySizeOfVariant& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const { return base::HashT(value); }
	};

	DECLARE_QUERY(QuerySizeOfVariant, KeyFor_QuerySizeOfVariant, usize)

	class ClassInfoImpl;

	struct KeyFor_QuerySizeOfClass {
		const ts::internal::ClassInfoImpl* value;
		KeyFor_QuerySizeOfClass() = delete;
		KeyFor_QuerySizeOfClass(const ts::internal::ClassInfoImpl* value): value(value) {}
		auto operator<=>(const KeyFor_QuerySizeOfClass& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const { return base::HashT(value); }
	};

	DECLARE_QUERY(QuerySizeOfClass, KeyFor_QuerySizeOfClass, usize)
}