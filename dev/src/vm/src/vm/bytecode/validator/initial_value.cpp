#include "initial_value.hpp"

#include "errors.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/bytecode/const_pool_visitor.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

namespace vm::code::detail {

namespace {

	class InitialValueValidator: public ConstVisitor {
	public:
		InitialValueValidator(
			valid_type::ValidTypeID           type_id,
			const valid_type::ValidTypeMap&   types,
			base::StrID                       global_name
		):
		      type_id(type_id),
		      types(types),
		      global_name(global_name) {}

		void visitConstantU64(const ConstantU64&) override {
			auto type_ref = types.at(type_id);

			if (!type_ref->isKind<valid_type::finalized::Primitive>())
				throw InitialValueTypeMismatchError(global_name, type_ref->getName());

			const auto& primitive = type_ref->getKindAs<valid_type::finalized::Primitive>();
			if (primitive->size > Bytes(8))
				throw InitialValueTypeMismatchError(global_name, type_ref->getName());
		}

		void visitConstantClass(const ConstantClass& value) override {
			auto type_ref = types.at(type_id);

			if (!type_ref->isKind<valid_type::finalized::Structure>())
				throw InitialValueTypeMismatchError(global_name, type_ref->getName());

			const auto& structure = type_ref->getKindAs<valid_type::finalized::Structure>();

			for (const auto& [field_name, field_value]: value.fields) {
				auto field_type_opt = structure->fields.atMaybe(field_name);
				if (!field_type_opt)
					throw InitialValueTypeMismatchError(global_name, type_ref->getName());

				InitialValueValidator field_validator(field_type_opt.value()->type, types, global_name);
				field_value->acceptVisitor(field_validator);
			}
		}

		void visitConstantFixedSizeTable(const ConstantFixedSizeTable& value) override {
			auto type_ref = types.at(type_id);

			if (!type_ref->isKind<valid_type::finalized::FixedSizeTable>())
				throw InitialValueTypeMismatchError(global_name, type_ref->getName());

			const auto& fst = type_ref->getKindAs<valid_type::finalized::FixedSizeTable>();
			if (value.elements.size() != fst->element_count)
				throw InitialValueTypeMismatchError(global_name, type_ref->getName());

			for (const auto& element: value.elements) {
				InitialValueValidator element_validator(fst->inner, types, global_name);
				element->acceptVisitor(element_validator);
			}
		}

	private:
		valid_type::ValidTypeID         type_id;
		const valid_type::ValidTypeMap& types;
		base::StrID                     global_name;
	};

} // namespace

void validateInitialValue(
	const ConstantValue&            value,
	valid_type::ValidTypeID         expected_type_id,
	const valid_type::ValidTypeMap& types,
	base::StrID                     global_name
) {
	InitialValueValidator validator(expected_type_id, types, global_name);
	value.data->acceptVisitor(validator);
}

} // namespace vm::code::detail