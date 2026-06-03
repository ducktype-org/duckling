#include "const_pool.hpp"

#include "const_pool_visitor.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace vm::code {
	Box<ConstantBase> ConstantU64::clone() const { return makeBox<ConstantU64>(*this); }

	Box<ConstantBase> ConstantClass::clone() const {
		auto cloned = makeBox<ConstantClass>();
		for (const auto& [name, field_val]: fields)
			cloned->fields.emplace_back(name, field_val->clone());
		return cloned;
	}

	Box<ConstantBase> ConstantFixedSizeTable::clone() const {
		auto cloned = makeBox<ConstantFixedSizeTable>();
		for (const auto& elem: elements) cloned->elements.push_back(elem->clone());
		return cloned;
	}

	ConstantValue::ConstantValue(const ConstantValue& other): data(other.data->clone()) {}

	ConstantValue& ConstantValue::operator=(const ConstantValue& other) {
		if (this == &other) return *this;
		data = other.data->clone();
		return *this;
	}

	void ConstantU64::acceptVisitor(ConstVisitor& v) const { v.visitConstantU64(*this); }

	void ConstantClass::acceptVisitor(ConstVisitor& v) const { v.visitConstantClass(*this); }

	void ConstantFixedSizeTable::acceptVisitor(ConstVisitor& v) const {
		v.visitConstantFixedSizeTable(*this);
	}

}  // namespace vm::code
