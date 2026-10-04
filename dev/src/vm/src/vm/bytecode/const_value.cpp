#include "const_value.hpp"

#include "const_value_visitor.hpp"

namespace vm::code {
	Box<ConstantBase> ConstantImmediate::clone() const { return makeBox<ConstantImmediate>(*this); }

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

	void ConstantImmediate::acceptVisitor(ConstVisitor& v) const {
		v.visitConstantImmediate(*this);
	}

	void ConstantClass::acceptVisitor(ConstVisitor& v) const { v.visitConstantClass(*this); }

	void ConstantFixedSizeTable::acceptVisitor(ConstVisitor& v) const {
		v.visitConstantFixedSizeTable(*this);
	}

}  // namespace vm::code
