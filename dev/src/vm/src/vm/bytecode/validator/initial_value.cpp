#include "initial_value.hpp"

#include "errors.hpp"

#include <vm/bytecode/const_value_visitor.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

namespace vm::code::detail {

	namespace {

		class InitialValueValidator final: public ConstVisitorPanicky {
		public:
			InitialValueValidator(
				valid_type::ValidTypeID         type_id,
				const valid_type::ValidTypeMap& types,
				Identifier                      global_name
			):
				  type_ref(types.at(type_id)),
				  types(types),
				  global_name(global_name) {}

#define THROW_ERROR(ADDITIONAL_CONTEXT) \
	throw InitialValueTypeMismatchError(global_name, type_ref->getName(), ADDITIONAL_CONTEXT);

			void visitConstantImmediate(const ConstantImmediate& value) override {
				if (!type_ref->isKind<valid_type::finalized::Primitive>())
					THROW_ERROR("Got immediate instead.");

				const auto& primitive = type_ref->getKindAs<valid_type::finalized::Primitive>();
				if (primitive->size != value.size)
					THROW_ERROR(base::strConcat(
						"Primitive of incorrect size. Expected ",
						primitive->size.asInt(),
						" bytes, got ",
						value.size.asInt(),
						" bytes."
					));
			}

			void visitConstantClass(const ConstantClass& value) override {
				if (!type_ref->isKind<valid_type::finalized::Structure>())
					THROW_ERROR("Got class instead.")

				const auto& structure = type_ref->getKindAs<valid_type::finalized::Structure>();

				if (value.fields.size() != structure->fields.size())
					THROW_ERROR(base::strConcat(
						"Got incorrect number of fields. Expected ",
						structure->fields.size(),
						", got ",
						value.fields.size()
					));

				std::unordered_set<base::StrID> visited_names;

				for (const auto& [field_name, field_value]: value.fields) {
					auto field_type_opt = structure->fields.atMaybe(field_name);
					if (!field_type_opt)
						THROW_ERROR(base::strConcat("Field '", field_name, "' does not exist."));
					if (visited_names.contains(field_name))
						THROW_ERROR(base::strConcat("Field '", field_name, "' duplicated."));
					visited_names.insert(field_name);

					InitialValueValidator field_validator(
						field_type_opt.value()->type, types, global_name
					);
					field_value->acceptVisitor(field_validator);
				}
			}

			void visitConstantFixedSizeTable(const ConstantFixedSizeTable& value) override {
				if (!type_ref->isKind<valid_type::finalized::FixedSizeTable>())
					THROW_ERROR("Got fixed size table instead.");

				const auto& fst = type_ref->getKindAs<valid_type::finalized::FixedSizeTable>();
				if (value.elements.size() != fst->element_count)
					THROW_ERROR(base::strConcat(
						"Got incorrect number of elements. Expected ",
						fst->element_count,
						", got ",
						value.elements.size()
					));

				for (const auto& element: value.elements) {
					InitialValueValidator element_validator(fst->inner, types, global_name);
					element->acceptVisitor(element_validator);
				}
			}

		private:
			CRef<valid_type::ValidType>     type_ref;
			const valid_type::ValidTypeMap& types;
			Identifier                      global_name;
		};

	}  // namespace

	void validateInitialValue(
		const ConstantValue&            value,
		valid_type::ValidTypeID         expected_type_id,
		const valid_type::ValidTypeMap& types,
		Identifier                      global_name
	) {
		InitialValueValidator validator(expected_type_id, types, global_name);
		value.data->acceptVisitor(validator);
	}

}  // namespace vm::code::detail
