// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "memory.hpp"

#include <vm/bytecode/const_value.hpp>
#include <vm/bytecode/const_value_visitor.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>

namespace vm {

	class ConstInitializationVisitor: public code::ConstVisitor {
		void visitConstantImmediate(const code::ConstantImmediate& value) override {
			CORE_ASSERT(
				current_offset.asInt() + value.size.asInt() <= data.size(), "Block overflow"
			);
			std::memcpy(
				data.getBegin() + current_offset.asInt(), value.content.data(), value.size.asInt()
			);
		}

		void visitConstantClass(const code::ConstantClass& value) override {
			for (auto& [field_name, field_value]: value.fields) {
				auto new_type = current_type->getFieldTypeByName(field_name).value();
				auto new_offset
					= current_offset + current_type->getFieldOffsetByName(field_name).value();
				ConstInitializationVisitor visitor{ new_type, new_offset, data };
				field_value->acceptVisitor(visitor);
			}
		}

		void visitConstantFixedSizeTable(const code::ConstantFixedSizeTable& value) override {
			auto elem_type    = current_type->getInnerType().value();
			auto elem_size    = elem_type->getSize();
			auto array_offset = Bytes(0);
			for (auto& elem: value.elements) {
				ConstInitializationVisitor visitor{ elem_type, current_offset + array_offset, data };
				elem->acceptVisitor(visitor);
				array_offset += elem_size;
			}
		}

	public:
		TypeCRef         current_type;
		Bytes            current_offset;
		base::ModRawView data;

		ConstInitializationVisitor(
			TypeCRef current_type, Bytes current_offset, base::ModRawView data
		):
			  current_type(current_type),
			  current_offset(current_offset),
			  data(data) {}
	};

	template<>
	void Memory::initializeBlockFromConstValue(
		Ref<Block> block, const code::ConstantValue& const_value
	) {
		TypeCRef                   block_type = block->data.element_type;
		ConstInitializationVisitor visitor{ block_type, Bytes(0), block->data.view };
		const_value.data->acceptVisitor(visitor);
	}
}
