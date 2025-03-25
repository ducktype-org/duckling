#pragma once

#include "vm/program/element_base.hpp"
#include <base/string_id.hpp>
#include <ostream>

namespace vm::program {
	struct PrimitiveType: ElementBase {
		base::StrID name;
		usize       size{};

		void dprint(std::ostream& out) const {
			out << "primitive {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    size: " << size << "\n";
			out << "}";
		}
	};

	struct PointerType: ElementBase {
		base::StrID name;
		base::StrID inner;

		void dprint(std::ostream& out) const {
			out << "pointer {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "}";
		}
	};

	struct StaticTableType: ElementBase {
		base::StrID name;
		base::StrID inner;
		usize       table_size;

		void dprint(std::ostream& out) const {
			out << "static_table {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "    table_size: " << table_size << "\n";
			out << "}";
		}
	};

	struct DynamicTableType: ElementBase {
		base::StrID name;
		base::StrID inner;

		void dprint(std::ostream& out) const {
			out << "dynamic_table {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "}";
		}
	};

	struct Field: ElementBase {
		base::StrID name;
		base::StrID type;
	};

	struct DataType: ElementBase {
		base::StrID        name;
		std::vector<Field> fields;

		void dprint(std::ostream& out) const {
			out << "data {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    fields: [";
			for (auto& field: fields)
				out << field.name.strView() << ": " << field.type.strView() << ", ";
			out << "]\n";
			out << "}";
		}
	};

	struct VariantType: ElementBase {
		base::StrID              name;
		std::vector<base::StrID> variant_alternatives;

		void dprint(std::ostream& out) const {
			out << "variant {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    alternatives: [";
			for (auto& alt: variant_alternatives) out << alt.strView() << ", ";
			out << "]\n";
			out << "}";
		}
	};

	struct FunctionType: ElementBase {
		base::StrID              name;
		std::vector<base::StrID> parameters;
		base::StrID              result;

		void dprint(std::ostream& out) const {
			out << "function {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    parameters: [";
			for (auto& param: parameters) out << param.strView() << ", ";
			out << "]\n";
			out << "    result: " << result.strView() << "\n";
			out << "}";
		}
	};

	using TypeOfData = std::variant<
		PrimitiveType,
		PointerType,
		StaticTableType,
		DynamicTableType,
		DataType,
		VariantType,
		FunctionType>;

	inline base::StrID typeName(const TypeOfData& type) {
		return std::visit([](const auto& t) { return t.name; }, type);
	}
}
