#include "lower.hpp"

#include "identifiers.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace c_import {

	namespace {

		template<typename... Ts>
		struct Overloaded: Ts... {
			using Ts::operator()...;
		};

		using TextOrReason = std::expected<std::string, std::string>;

		constexpr std::string_view BITFIELD_ISSUE      = "#3647";
		constexpr std::string_view KEYWORD_ISSUE       = "#3649";
		constexpr std::string_view VARIADIC_ISSUE      = "#3271";
		constexpr std::string_view CALLBACK_ISSUE      = "#3650";
		constexpr std::string_view POINTER_CYCLE_ISSUE = "#2616";
		// @TODO: #1498 `extern("C")` classes reject 128-bit integers, so no storage type is aligned
		// to 16.
		constexpr std::uint64_t MAX_BLOB_ALIGN    = 8;
		constexpr std::uint64_t MAX_BYTE_ASSEMBLY = 8;

		std::uint64_t alignUp(std::uint64_t value, std::uint64_t align) {
			return align == 0 ? value : (value + align - 1) / align * align;
		}

		/// A field of a record with anonymous members spliced in, at its offset in the outermost
		/// record.
		struct FlatField final {
			std::string                  name;
			CTypeRef                     type;
			std::uint64_t                offset_bits;
			std::optional<std::uint32_t> bit_width;
			std::uint64_t                size;
			std::uint64_t                align;
		};

		enum class Demand : std::uint8_t {
			None,
			Pointer,
			Value,
		};

		enum class PlanKind : std::uint8_t {
			Pending,
			Opaque,
			Natural,
			Blob,
			Skipped,
		};

		struct RecordPlan final {
			PlanKind    kind = PlanKind::Pending;
			std::string reason;
		};

		class Lowerer final {
		public:
			Lowerer(const CModel& model, const LowerOptions& options):
				  model(model),
				  options(options),
				  plans(model.records.size()),
				  demands(model.records.size(), Demand::None),
				  names(model.records.size()),
				  by_value(model.records.size(), false) {}

			DkModule run() {
				nameRecords();
				collectDemands();
				for (std::size_t i = 0; i < model.records.size(); ++i)
					if (demands[i] != Demand::None) plan(i);
				findCycles();

				emitRecords();
				emitFunctions();
				emitEnums();
				emitConstants();
				for (const auto& unreadable: model.unreadable)
					if (wanted(unreadable.location, unreadable.name))
						skip(unreadable.name, unreadable.reason);
				return std::move(out);
			}

		private:
			const CModel&            model;
			const LowerOptions&      options;
			std::vector<RecordPlan>  plans;
			std::vector<Demand>      demands;
			std::vector<std::string> names;
			std::set<std::string>    taken;
			/// Every C name that becomes a module-level Duckling name; a field or parameter named
			/// like one of them is ambiguous in Duckling, so it is renamed.
			std::set<std::string> top_level;
			/// Records something needs by value (a field or a signature), not only through a pointer.
			std::vector<bool> by_value;
			DkModule          out;
			/// Strongly connected component of each record over the references of its fields.
			std::vector<std::size_t> components;
			/// The record whose fields are being written, whose pointers may close a cycle.
			std::optional<std::size_t> cycle_owner;
			mutable bool               broke_cycle = false;

			// ============================== naming ==============================

			bool wanted(const CLocation& location, std::string_view name) const {
				return location.in_requested_headers
				    && passesFilters(name, options.include, options.exclude);
			}

			void nameRecords() {
				// Ordinary identifiers share one namespace in Duckling, while C keeps record tags
				// apart; a record is the one renamed, since its name is never linked.
				std::set<std::string> ordinary;
				for (const auto& function: model.functions) ordinary.insert(function.name);
				for (const auto& constant: model.constants) ordinary.insert(constant.name);
				for (const auto& enumeration: model.enums)
					for (const auto& enumerator: enumeration.enumerators)
						ordinary.insert(enumerator.name);

				std::map<std::size_t, std::pair<std::size_t, std::size_t>> anonymous_parent;
				for (std::size_t parent = 0; parent < model.records.size(); ++parent) {
					std::size_t anonymous_index = 0;
					for (const auto& field: model.records[parent].fields) {
						const auto* ref = std::get_if<CRecordRef>(&field.type->kind);
						if (field.name.empty() && ref && model.records[ref->index].anonymous)
							anonymous_parent[ref->index] = { parent, anonymous_index++ };
					}
				}

				std::set<std::string> used;
				for (std::size_t i = 0; i < model.records.size(); ++i) {
					std::string name = recordBaseName(i, anonymous_parent);
					if (ordinary.contains(name))
						name += model.records[i].is_union ? "_union" : "_struct";
					name               = usableName(name);
					std::string unique = name;
					for (std::size_t n = 2; used.contains(unique); ++n)
						unique = std::format("{}_{}", name, n);
					used.insert(unique);
					names[i] = std::move(unique);
				}
				top_level = std::move(ordinary);
				top_level.insert(used.begin(), used.end());
			}

			std::string recordBaseName(
				std::size_t                                                       i,
				const std::map<std::size_t, std::pair<std::size_t, std::size_t>>& anonymous_parent
			) const {
				const auto& record = model.records[i];
				if (!record.anonymous) return record.name;
				auto parent = anonymous_parent.find(i);
				if (parent == anonymous_parent.end()) return std::format("__anon_record_{}", i);
				return std::format(
					"{}__anon{}",
					recordBaseName(parent->second.first, anonymous_parent),
					parent->second.second
				);
			}

			bool claim(const std::string& name) { return taken.insert(name).second; }

			// @TODO: #2135 A field or parameter named like a module-level name is ambiguous, so it
			// is renamed.
			/// A field or parameter name: usable, and distinct from every module-level name.
			[[nodiscard]] std::string localName(std::string_view name) const {
				std::string result = usableName(name);
				while (top_level.contains(result)) result += '_';
				return result;
			}

			void skip(std::string name, std::string reason) {
				out.skipped.push_back({ std::move(name), std::move(reason) });
			}

			// ============================== demands ==============================

			void demandType(const CTypeRef& type, bool through_pointer) {
				std::visit(
					Overloaded{
						[&](const CPointer& p) { demandType(p.pointee, true); },
						[&](const CArray& a) { demandType(a.element, through_pointer); },
						[&](const CRecordRef& r) {
							if (!through_pointer) by_value[r.index] = true;
							demandRecord(r.index, through_pointer ? Demand::Pointer : Demand::Value);
						},
						[](const auto&) {},
					},
					type->kind
				);
			}

			void demandRecord(std::size_t i, Demand demand) {
				// A complete record keeps its layout even when only pointed to, so it can be allocated.
				if (model.records[i].complete) demand = Demand::Value;
				if (demands[i] >= demand) return;
				const bool first_value = demand == Demand::Value;
				demands[i]             = demand;
				if (!first_value) return;
				for (const auto& field: model.records[i].fields) demandType(field.type, false);
			}

			void collectDemands() {
				for (std::size_t i = 0; i < model.records.size(); ++i) {
					const auto& record = model.records[i];
					if (!record.anonymous && wanted(record.location, record.name))
						demandRecord(i, record.complete ? Demand::Value : Demand::Pointer);
				}
				for (const auto& function: model.functions) {
					if (!wanted(function.location, function.name)) continue;
					demandType(function.return_type, false);
					for (const auto& param: function.params) demandType(param.type, false);
				}
			}

			// ============================== planning ==============================

			void plan(std::size_t i) {
				auto& p = plans[i];
				if (p.kind != PlanKind::Pending) return;
				const auto& record = model.records[i];

				if (!record.complete) {
					p.kind = PlanKind::Opaque;
					return;
				}
				// Guards against a record that contains itself by value, which C rules out.
				p.kind   = PlanKind::Skipped;
				p.reason = "record contains itself";
				for (const auto& field: record.fields) planByValue(field.type);

				if (auto natural = naturalFields(i); natural.has_value()) {
					p.kind = PlanKind::Natural;
					p.reason.clear();
					return;
				}
				// @TODO: #3647 Layout blobs stand in for unions, bitfields and packed or
				// over-aligned records.
				if (auto storage = blobStorageType(record); storage.has_value()) {
					p.kind = PlanKind::Blob;
					p.reason.clear();
				} else if (!by_value[i]) {
					// Nothing needs its layout, so a handle keeps its pointers typed.
					p.kind   = PlanKind::Opaque;
					p.reason = storage.error();
				} else {
					p.reason = storage.error();
				}
			}

			void planByValue(const CTypeRef& type) {
				if (const auto* array = std::get_if<CArray>(&type->kind))
					planByValue(array->element);
				else if (const auto* ref = std::get_if<CRecordRef>(&type->kind))
					plan(ref->index);
			}

			/// Whether a pointer from the record being written to record `target` closes a cycle.
			// @TODO: #2616 Pointer layouts query their pointee eagerly, so a cycle of them cannot
			// be laid out.
			[[nodiscard]] bool closesCycle(std::size_t target) const {
				return cycle_owner.has_value() && plans[target].kind == PlanKind::Natural
				    && components[*cycle_owner] == components[target];
			}

			void collectReferences(const CTypeRef& type, std::vector<std::size_t>& result) const {
				std::visit(
					Overloaded{
						[&](const CPointer& p) { collectReferences(p.pointee, result); },
						[&](const CArray& a) { collectReferences(a.element, result); },
						[&](const CRecordRef& r) {
							if (plans[r.index].kind == PlanKind::Natural) result.push_back(r.index);
						},
						[](const auto&) {},
					},
					type->kind
				);
			}

			/// Tarjan's algorithm over the natural records, whose layouts query their fields'.
			void findCycles() {
				const std::size_t                     count = model.records.size();
				std::vector<std::vector<std::size_t>> edges(count);
				for (std::size_t i = 0; i < count; ++i) {
					if (plans[i].kind != PlanKind::Natural) continue;
					std::vector<FlatField> flat;
					flattenAll(i, 0, flat);
					for (const auto& field: flat) collectReferences(field.type, edges[i]);
				}

				constexpr std::size_t UNVISITED = std::numeric_limits<std::size_t>::max();
				components.assign(count, UNVISITED);
				std::vector<std::size_t> index(count, UNVISITED);
				std::vector<std::size_t> low(count, 0);
				std::vector<bool>        on_stack(count, false);
				std::vector<std::size_t> stack;
				std::size_t              next_index     = 0;
				std::size_t              next_component = 0;

				std::function<void(std::size_t)> connect = [&](std::size_t v) {
					index[v] = low[v] = next_index++;
					stack.push_back(v);
					on_stack[v] = true;
					for (auto w: edges[v]) {
						if (index[w] == UNVISITED) {
							connect(w);
							low[v] = std::min(low[v], low[w]);
						} else if (on_stack[w]) {
							low[v] = std::min(low[v], index[w]);
						}
					}
					if (low[v] != index[v]) return;
					std::size_t w = 0;
					do {
						w = stack.back();
						stack.pop_back();
						on_stack[w]   = false;
						components[w] = next_component;
					} while (w != v);
					++next_component;
				};
				for (std::size_t v = 0; v < count; ++v)
					if (index[v] == UNVISITED) connect(v);
			}

			static std::expected<std::string, std::string> blobStorageType(const CRecord& record) {
				if (record.size == 0) return std::unexpected("record has no size");
				if (record.align > MAX_BLOB_ALIGN || !std::has_single_bit(record.align))
					return std::unexpected(
						std::format("alignment {} needs 128-bit storage (#1498)", record.align)
					);
				if (record.size % record.align != 0)
					return std::unexpected("size is not a multiple of the alignment");
				return std::format("u{}[{}]", record.align * 8, record.size / record.align);
			}

			// @TODO: #3648 Anonymous members are spliced into their parent or reached through accessors.
			bool isAnonymousMember(const CField& field) const {
				const auto* ref = std::get_if<CRecordRef>(&field.type->kind);
				return field.name.empty() && ref && model.records[ref->index].anonymous;
			}

			/**
			 * @brief The fields of record `i` with anonymous structs spliced in; an anonymous
			 * union stays a single field of its (blob) type.
			 */
			std::expected<std::vector<FlatField>, std::string> flattenForNatural(
				std::size_t i, std::uint64_t base_bits
			) {
				std::vector<FlatField> result;
				for (const auto& field: model.records[i].fields) {
					if (field.bit_width.has_value()) return std::unexpected("has bitfields");
					if (isAnonymousMember(field)) {
						auto inner = std::get<CRecordRef>(field.type->kind).index;
						if (!model.records[inner].is_union) {
							auto inner_fields
								= flattenForNatural(inner, base_bits + field.offset_bits);
							if (!inner_fields) return inner_fields;
							std::ranges::move(*inner_fields, std::back_inserter(result));
							continue;
						}
					} else if (field.name.empty()) {
						return std::unexpected("has an unnamed field");
					}
					result.push_back({ field.name,
					                   field.type,
					                   base_bits + field.offset_bits,
					                   field.bit_width,
					                   field.size,
					                   field.align });
				}
				return result;
			}

			/// The fields of the class for record `i`, if Duckling's natural layout reproduces it.
			std::expected<std::vector<DkNameType>, std::string> naturalFields(std::size_t i) {
				const auto& record = model.records[i];
				if (record.is_union) return std::unexpected("union");
				auto flat = flattenForNatural(i, 0);
				if (!flat) return std::unexpected(flat.error());
				if (flat->empty()) return std::unexpected("record has no fields");

				std::uint64_t           offset    = 0;
				std::uint64_t           max_align = 1;
				std::set<std::string>   seen;
				std::vector<DkNameType> fields;
				std::size_t             anonymous_index = 0;
				for (const auto& field: *flat) {
					if (field.align == 0) return std::unexpected("field has no alignment");
					offset = alignUp(offset, field.align);
					if (offset * 8 != field.offset_bits)
						return std::unexpected(
							"layout is not the natural one (packed or explicitly aligned)"
						);
					offset += field.size;
					max_align = std::max(max_align, field.align);

					auto type = fieldTypeText(field.type);
					if (!type)
						return std::unexpected(
							std::format("field `{}`: {}", field.name, type.error())
						);

					std::string name = field.name.empty()
					                     ? std::format("__anon{}", anonymous_index++)
					                     : localName(field.name);
					if (!seen.insert(name).second)
						return std::unexpected(std::format(
							"field `{}` is declared twice once anonymous members are flattened", name
						));
					fields.push_back({ std::move(name), std::move(*type) });
				}
				if (alignUp(offset, max_align) != record.size || max_align != record.align)
					return std::unexpected(
						"layout is not the natural one (packed or explicitly aligned)"
					);
				return fields;
			}

			// ============================== type text ==============================

			static std::expected<std::string, std::string> scalarText(
				CScalar scalar, bool char_is_signed
			) {
				switch (scalar.kind) {
				case ScalarKind::Bool:
					return "bool";
				case ScalarKind::Char:
					// Duckling `char` is unsigned 8-bit, so a signed C `char` would be zero-extended.
					return char_is_signed ? "i8" : "u8";
				case ScalarKind::SignedInt:
				case ScalarKind::UnsignedInt:
					if (scalar.bits > 64)
						return std::unexpected(
							std::format("{}-bit integers have no mapping (#1498)", scalar.bits)
						);
					return std::format(
						"{}{}", scalar.kind == ScalarKind::SignedInt ? 'i' : 'u', scalar.bits
					);
				case ScalarKind::Float:
					if (scalar.bits != 32 && scalar.bits != 64)
						return std::unexpected(
							std::format("{}-bit floats have no mapping (#1498)", scalar.bits)
						);
					return std::format("f{}", scalar.bits);
				}
				return std::unexpected("unknown scalar");
			}

			std::expected<std::string, std::string> scalarText(CScalar scalar) const {
				return scalarText(scalar, model.char_is_signed);
			}

			/// `cptr` to `pointee`; anything Duckling cannot point at is an opaque byte pointer.
			std::string pointerText(const CTypeRef& pointee) const {
				auto inner = std::visit(
					Overloaded{
						[&](const CScalar& s) -> std::string {
							if (s.kind == ScalarKind::Char) return "char";
							return scalarText(s).value_or("u8");
						},
						[&](const CPointer& p) { return pointerText(p.pointee); },
						[&](const CArray& a) -> std::string {
							// A pointer to an array addresses its first element.
							auto element = pointerText(a.element);
							return element.substr(std::string_view{ "cptr " }.size());
						},
						[&](const CRecordRef& r) -> std::string {
							auto kind = plans[r.index].kind;
							if (closesCycle(r.index)) {
								broke_cycle = true;
								return "u8";
							}
							if (kind == PlanKind::Opaque || kind == PlanKind::Natural
					            || kind == PlanKind::Blob)
								return names[r.index];
							return "u8";
						},
						// @TODO: #3650 A function pointer is an opaque `cptr u8` until it can
				        // be called.
						[](const auto&) -> std::string { return "u8"; },
					},
					pointee->kind
				);
				return "cptr " + inner;
			}

			TextOrReason fieldTypeText(const CTypeRef& type) const {
				return std::visit(
					Overloaded{
						[&](const CScalar& s) { return scalarText(s); },
						[&](const CPointer& p) -> TextOrReason { return pointerText(p.pointee); },
						[&](const CArray& a) -> TextOrReason {
							if (a.count == 0)
								return std::unexpected("flexible array member (#3647)");
							// Multi-dimensional arrays flatten into one: the layout is identical
					        // and no index order has to be guessed.
							std::uint64_t count   = a.count;
							CTypeRef      element = a.element;
							while (const auto* inner = std::get_if<CArray>(&element->kind)) {
								if (inner->count == 0)
									return std::unexpected("flexible array member (#3647)");
								count *= inner->count;
								element = inner->element;
							}
							auto text = fieldTypeText(element);
							if (!text) return text;
							// `cptr T[N]` is a pointer to an array, so an array of pointers needs
					        // parentheses.
							if (text->starts_with("cptr "))
								return std::format("({})[{}]", *text, count);
							return std::format("{}[{}]", *text, count);
						},
						[&](const CRecordRef& r) -> TextOrReason {
							const auto& p = plans[r.index];
							if (p.kind == PlanKind::Natural || p.kind == PlanKind::Blob)
								return names[r.index];
							if (p.kind == PlanKind::Opaque)
								return std::unexpected(
									std::format("`{}` is incomplete", names[r.index])
								);
							return std::unexpected(
								std::format("`{}` is skipped: {}", names[r.index], p.reason)
							);
						},
						[](const CVoid&) -> TextOrReason { return std::unexpected("`void` value"); },
						[](const CFunctionType&) -> TextOrReason {
							return std::unexpected("function type");
						},
						[](const CUnsupported& u) -> TextOrReason {
							return std::unexpected(u.reason);
						},
					},
					type->kind
				);
			}

			bool containsFloat(const CTypeRef& type) const {
				return std::visit(
					Overloaded{
						[](const CScalar& s) { return s.kind == ScalarKind::Float; },
						[&](const CArray& a) { return containsFloat(a.element); },
						[&](const CRecordRef& r) {
							return std::ranges::any_of(
								model.records[r.index].fields,
								[&](const CField& f) { return containsFloat(f.type); }
							);
						},
						[](const auto&) { return false; },
					},
					type->kind
				);
			}

			bool containsBlob(const CTypeRef& type) const {
				return std::visit(
					Overloaded{
						[&](const CArray& a) { return containsBlob(a.element); },
						[&](const CRecordRef& r) {
							if (plans[r.index].kind == PlanKind::Blob) return true;
							return std::ranges::any_of(
								model.records[r.index].fields,
								[&](const CField& f) { return containsBlob(f.type); }
							);
						},
						[](const auto&) { return false; },
					},
					type->kind
				);
			}

			/// The type of a parameter or return value, as passed through the C ABI.
			TextOrReason signatureTypeText(const CTypeRef& type) const {
				if (const auto* array = std::get_if<CArray>(&type->kind))
					return pointerText(array->element);
				if (std::holds_alternative<CFunctionType>(type->kind))
					return std::unexpected(
						std::format("function type in a signature ({})", CALLBACK_ISSUE)
					);
				if (const auto* ref = std::get_if<CRecordRef>(&type->kind)) {
					auto text = fieldTypeText(type);
					if (!text) return text;
					// A blob is only its storage, so it is classified as integers: that matches the C
					// record when it is passed in memory anyway, or holds no floating-point member.
					const auto& record = model.records[ref->index];
					if (containsBlob(type) && record.size <= 16 && containsFloat(type))
						return std::unexpected(std::format(
							"passes `{}` by value, whose ABI class a layout blob cannot reproduce "
							"({})",
							names[ref->index],
							BITFIELD_ISSUE
						));
					return text;
				}
				return fieldTypeText(type);
			}

			// ============================== records ==============================

			void emitRecords() {
				for (std::size_t i = 0; i < model.records.size(); ++i) {
					const auto& p      = plans[i];
					const auto& record = model.records[i];
					const auto& name   = names[i];
					// An anonymous struct is spliced into its parent, or reached through the
					// parent's accessors, so it needs no class of its own.
					if (record.anonymous && !record.is_union) continue;
					switch (p.kind) {
					case PlanKind::Pending:
						break;
					case PlanKind::Opaque:
						claim(name);
						out.classes.push_back({
							name,
							{ { .name = "_opaque", .type = "u8" } },
							p.reason.empty()
								? std::string{ "Opaque handle: only use it through a `cptr`." }
								: std::format(
									  "Opaque handle, since its layout cannot be expressed ({}): "
									  "only use it "
									  "through a `cptr`.",
									  p.reason
								  ),
						});
						break;
					case PlanKind::Natural: {
						claim(name);
						cycle_owner = i;
						broke_cycle = false;
						auto fields = naturalFields(i).value();
						cycle_owner.reset();
						out.classes.push_back({
							name,
							std::move(fields),
							broke_cycle
								? std::format(
									  "Pointers to records that point back to `{}` are `cptr u8`: "
									  "a pointer cycle between classes cycles the compiler ({}).",
									  name,
									  POINTER_CYCLE_ISSUE
								  )
								: std::string{},
						});
						out.layouts.push_back({ name, record.size, record.align });
						emitAnonymousUnionAccessors(i, i, 0);
						break;
					}
					case PlanKind::Blob:
						claim(name);
						out.classes.push_back({
							name,
							{ { .name = "_storage", .type = blobStorageType(record).value() } },
							std::format(
								"Layout blob of {} `{}`{}: reach its members through the "
								"generated `{}_*` functions.",
								record.is_union ? "union" : "struct",
								record.anonymous ? name : record.name,
								blobReason(i),
								name
							),
						});
						out.layouts.push_back({ name, record.size, record.align });
						emitBlobAccessors(i);
						break;
					case PlanKind::Skipped:
						if (!record.anonymous) skip(name, p.reason);
						break;
					}
				}
			}

			std::string blobReason(std::size_t i) {
				if (model.records[i].is_union) return {};
				auto natural = naturalFields(i);
				return natural ? std::string{} : std::format(" ({})", natural.error());
			}

			/// Every member of record `i`, anonymous ones spliced in, at offsets from `base_bits`.
			void flattenAll(std::size_t i, std::uint64_t base_bits, std::vector<FlatField>& result)
				const {
				for (const auto& field: model.records[i].fields) {
					if (isAnonymousMember(field)) {
						flattenAll(
							std::get<CRecordRef>(field.type->kind).index,
							base_bits + field.offset_bits,
							result
						);
						continue;
					}
					if (field.name.empty()) continue;
					result.push_back({ field.name,
					                   field.type,
					                   base_bits + field.offset_bits,
					                   field.bit_width,
					                   field.size,
					                   field.align });
				}
			}

			/// Accessors on record `owner` for the members of the anonymous unions in record `i`.
			void emitAnonymousUnionAccessors(
				std::size_t owner, std::size_t i, std::uint64_t base_bits
			) {
				for (const auto& field: model.records[i].fields) {
					if (!isAnonymousMember(field)) continue;
					auto inner = std::get<CRecordRef>(field.type->kind).index;
					if (!model.records[inner].is_union) {
						// An anonymous struct is spliced into the class, but its anonymous unions
						// are not.
						emitAnonymousUnionAccessors(owner, inner, base_bits + field.offset_bits);
						continue;
					}
					std::vector<FlatField> members;
					flattenAll(inner, base_bits + field.offset_bits, members);
					for (const auto& member: members) emitAccessors(owner, member);
				}
			}

			void emitBlobAccessors(std::size_t i) {
				const auto&            record = model.records[i];
				std::vector<FlatField> members;
				flattenAll(i, 0, members);
				for (const auto& member: members)
					if (record.is_union && member.offset_bits == 0 && !member.bit_width)
						emitPointerView(i, member);
					else
						emitAccessors(i, member);
			}

			// @TODO: #2135 Generated parameters and locals are prefixed with `__dk_`, since one
			// named like a C function (ncurses has `raw`) is ambiguous.
			/// `R_as_m(p: cptr R) -> cptr M`, which works on every backend since it is only a cast.
			void emitPointerView(std::size_t i, const FlatField& member) {
				const auto& record    = names[i];
				auto        view_name = std::format("{}_as_{}", record, member.name);
				auto        target    = pointerText(member.type);
				if (std::holds_alternative<CFunctionType>(member.type->kind)) {
					skip(view_name, std::format("function-typed member ({})", CALLBACK_ISSUE));
					return;
				}
				if (!claim(view_name)) {
					skip(view_name, "name is already taken");
					return;
				}
				out.functions.push_back({
					view_name,
					{ { .name = "__dk_p", .type = "cptr " + record } },
					target,
					{ std::format("return __dk_p as {};", target) },
				});
			}

			// @TODO: #3662 A member away from offset 0 gets a getter and setter instead of a
			// pointer, until the DVM backend can lower `&` of a place reached through a `cptr`.
			void emitAccessors(std::size_t i, const FlatField& member) {
				const auto& record    = names[i];
				auto        getter    = std::format("{}_get_{}", record, member.name);
				auto        setter    = std::format("{}_set_{}", record, member.name);
				auto        skip_both = [&](const std::string& reason) {
                    skip(std::format("{}.{}", record, member.name), reason);
				};

				auto type = fieldTypeText(member.type);
				if (!type) {
					skip_both(type.error());
					return;
				}
				if (taken.contains(getter) || taken.contains(setter)) {
					skip_both("accessor name is already taken");
					return;
				}

				const auto* scalar = std::get_if<CScalar>(&member.type->kind);
				const bool  is_aligned
					= !member.bit_width && member.offset_bits % 8 == 0
				   && (member.offset_bits / 8) % std::max<std::uint64_t>(member.align, 1) == 0;

				if (is_aligned) {
					emitViewAccessors(record, getter, setter, member, *type);
				} else if (scalar && scalar->kind != ScalarKind::Float) {
					if (!emitByteAccessors(record, getter, setter, member, *scalar, *type)) {
						skip_both(std::format("spans more than {} bytes", MAX_BYTE_ASSEMBLY));
						return;
					}
				} else {
					skip_both("misaligned non-integer member");
					return;
				}
				claim(getter);
				claim(setter);
			}

			/// Accessors through a class that places the member at its offset.
			void emitViewAccessors(
				const std::string& record,
				const std::string& getter,
				const std::string& setter,
				const FlatField&   member,
				const std::string& type
			) {
				auto                    view = std::format("{}__view_{}", record, member.name);
				std::vector<DkNameType> fields;
				if (auto pad = member.offset_bits / 8; pad > 0)
					fields.push_back({ "_pad", std::format("u8[{}]", pad) });
				fields.push_back({ "__dk_v", type });
				claim(view);
				out.classes.push_back({ view, std::move(fields), {} });

				out.functions.push_back({
					getter,
					{ { .name = "__dk_p", .type = "cptr " + record } },
					type,
					{ std::format("let __dk_view = __dk_p as cptr {};", view),
				      "return __dk_view[0].__dk_v;" },
				});
				out.functions.push_back({
					setter,
					{ { .name = "__dk_p", .type = "cptr " + record },
				      { .name = "__dk_value", .type = type } },
					"()",
					{ std::format("let __dk_view = __dk_p as cptr {};", view),
				      "__dk_view[0].__dk_v = __dk_value;" },
				});
			}

			/// Accessors assembling the member from single bytes, for bitfields and misaligned integers.
			bool emitByteAccessors(
				const std::string& record,
				const std::string& getter,
				const std::string& setter,
				const FlatField&   member,
				CScalar            scalar,
				const std::string& type
			) {
				const std::uint64_t width
					= member.bit_width.value_or(static_cast<std::uint32_t>(member.size * 8));
				if (width == 0) return true;
				const std::uint64_t first = member.offset_bits / 8;
				const std::uint64_t last  = (member.offset_bits + width - 1) / 8;
				const std::uint64_t count = last - first + 1;
				if (count > MAX_BYTE_ASSEMBLY) return false;
				const std::uint64_t shift = member.offset_bits % 8;
				const std::uint64_t mask  = width == 64 ? std::numeric_limits<std::uint64_t>::max()
				                                        : (std::uint64_t{ 1 } << width) - 1;
				const std::uint64_t clear = count == 8 && shift == 0 && width == 64
				                              ? 0
				                              : ~(mask << shift) & bytesMask(count);

				std::vector<std::string> read{ "let __dk_b = __dk_p as cptr u8;",
					                           "var __dk_raw: u64 = 0u64;" };
				for (std::uint64_t k = 0; k < count; ++k)
					read.push_back(std::format(
						"__dk_raw = __dk_raw | ((__dk_b[{}] as u64) << {}u64);", first + k, k * 8
					));

				std::vector<std::string> get = read;
				get.push_back(
					std::format("var __dk_bits: u64 = (__dk_raw >> {}u64) & {}u64;", shift, mask)
				);
				const bool is_signed = scalar.kind == ScalarKind::SignedInt
				                    || (scalar.kind == ScalarKind::Char && model.char_is_signed);
				if (scalar.kind == ScalarKind::Bool) {
					get.emplace_back("return __dk_bits != 0u64;");
				} else if (is_signed) {
					const std::uint64_t sign = std::uint64_t{ 1 } << (width - 1);
					get.push_back(std::format(
						"if ((__dk_bits & {}u64) != 0u64) {{ __dk_bits = __dk_bits | {}u64; }}",
						sign,
						~mask
					));
					get.push_back(std::format("return (__dk_bits as i64) as {};", type));
				} else {
					get.push_back(std::format("return __dk_bits as {};", type));
				}
				out.functions.push_back({ getter,
				                          { { .name = "__dk_p", .type = "cptr " + record } },
				                          type,
				                          std::move(get) });

				std::vector<std::string> set = read;
				if (scalar.kind == ScalarKind::Bool) {
					set.emplace_back("var __dk_bits: u64 = 0u64;");
					set.emplace_back("if (__dk_value) { __dk_bits = 1u64; }");
				} else if (is_signed) {
					set.push_back(std::format(
						"let __dk_bits: u64 = ((__dk_value as i64) as u64) & {}u64;", mask
					));
				} else {
					set.push_back(
						std::format("let __dk_bits: u64 = (__dk_value as u64) & {}u64;", mask)
					);
				}
				set.push_back(std::format(
					"__dk_raw = (__dk_raw & {}u64) | (__dk_bits << {}u64);", clear, shift
				));
				for (std::uint64_t k = 0; k < count; ++k)
					set.push_back(std::format(
						"__dk_b[{}] = ((__dk_raw >> {}u64) & 255u64) as u8;", first + k, k * 8
					));
				out.functions.push_back({ setter,
				                          { { .name = "__dk_p", .type = "cptr " + record },
				                            { .name = "__dk_value", .type = type } },
				                          "()",
				                          std::move(set) });
				return true;
			}

			static std::uint64_t bytesMask(std::uint64_t count) {
				return count >= 8 ? std::numeric_limits<std::uint64_t>::max()
				                  : (std::uint64_t{ 1 } << (count * 8)) - 1;
			}

			// ============================== functions ==============================

			void emitFunctions() {
				for (const auto& function: model.functions) {
					if (!wanted(function.location, function.name)) continue;
					if (auto reason = functionSkipReason(function); !reason.empty()) {
						skip(function.name, reason);
						continue;
					}
					DkFundecl   decl{ .name        = function.name,
						              .params      = {},
						              .return_type = std::nullopt };
					std::string failure;
					// @TODO: #3646 A `void` return is left out, since `-> void` miscompiles.
					if (!std::holds_alternative<CVoid>(function.return_type->kind)) {
						auto ret = signatureTypeText(function.return_type);
						if (!ret)
							failure = "return type: " + ret.error();
						else
							decl.return_type = *ret;
					}
					std::set<std::string> param_names;
					for (std::size_t k = 0; k < function.params.size() && failure.empty(); ++k) {
						const auto& param = function.params[k];
						auto        type  = signatureTypeText(param.type);
						if (!type) {
							failure = std::format("parameter {}: {}", k + 1, type.error());
							break;
						}
						std::string name
							= param.name.empty() ? std::format("arg{}", k) : localName(param.name);
						while (!param_names.insert(name).second) name += '_';
						decl.params.push_back({ std::move(name), std::move(*type) });
					}
					if (!failure.empty()) {
						skip(function.name, failure);
						continue;
					}
					if (!claim(function.name)) {
						skip(function.name, "name is already taken");
						continue;
					}
					out.fundecls.push_back(std::move(decl));
				}
			}

			static std::string functionSkipReason(const CFunction& function) {
				if (function.internal_linkage)
					return "`static` function: there is no symbol to link against";
				// @TODO: #3271 Variadic functions need a declaration per call signature.
				if (function.variadic) return std::format("variadic function ({})", VARIADIC_ISSUE);
				// @TODO: #3649 A symbol-name attribute would let these be declared under another name.
				if (isReservedName(function.name))
					return std::format("the linked name is a Duckling keyword ({})", KEYWORD_ISSUE);
				return {};
			}

			// ============================== constants ==============================

			static std::string integerLiteral(
				std::variant<std::int64_t, std::uint64_t> value,
				CScalar                                   scalar,
				const std::string&                        type
			) {
				if (scalar.kind == ScalarKind::Bool) {
					bool truthy = std::visit([](auto v) { return v != 0; }, value);
					return truthy ? "true" : "false";
				}
				if (const auto* u = std::get_if<std::uint64_t>(&value))
					return std::format("{}{}", *u, type);
				auto v = std::get<std::int64_t>(value);
				if (type.starts_with('u'))
					return std::format("{}{}", static_cast<std::uint64_t>(v), type);
				if (v == std::numeric_limits<std::int64_t>::min())
					return std::format("(-9223372036854775807{0} - 1{0})", type);
				if (v < 0 && type == "i32" && v == std::numeric_limits<std::int32_t>::min())
					return std::format("(-2147483647{0} - 1{0})", type);
				if (v < 0 && type == "i16" && v == std::numeric_limits<std::int16_t>::min())
					return std::format("(-32767{0} - 1{0})", type);
				if (v < 0 && type == "i8" && v == std::numeric_limits<std::int8_t>::min())
					return std::format("(-127{0} - 1{0})", type);
				return std::format("{}{}", v, type);
			}

			static std::optional<std::string> floatLiteral(double value, const std::string& type) {
				if (!std::isfinite(value)) return std::nullopt;
				std::array<char, 400> buffer{};
				auto [end, ec] = std::to_chars(
					buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed
				);
				if (ec != std::errc{}) return std::nullopt;
				std::string text{ buffer.data(), end };
				if (!text.contains('.')) text += ".0";
				if (type == "f32") text += "f32";
				return text;
			}

			void emitConstant(const std::string& c_name, CScalar scalar, const auto& value) {
				auto type = scalarText(scalar);
				if (!type) {
					skip(c_name, type.error());
					return;
				}
				std::optional<std::string> literal;
				std::visit(
					Overloaded{
						[&](double d) { literal = floatLiteral(d, *type); },
						[&](auto integer) { literal = integerLiteral(integer, scalar, *type); },
					},
					value
				);
				if (!literal) {
					skip(c_name, "value has no Duckling literal");
					return;
				}
				auto name = usableName(c_name);
				if (!claim(name)) {
					skip(c_name, "name is already taken");
					return;
				}
				out.constants.push_back({ std::move(name), std::move(*type), std::move(*literal) });
			}

			void emitEnums() {
				for (const auto& enumeration: model.enums) {
					if (!enumeration.location.in_requested_headers) continue;
					for (const auto& enumerator: enumeration.enumerators) {
						if (!passesFilters(enumerator.name, options.include, options.exclude))
							continue;
						std::visit(
							[&](auto v) {
								emitConstant(
									enumerator.name,
									enumeration.underlying,
									std::variant<std::int64_t, std::uint64_t, double>{ v }
								);
							},
							enumerator.value
						);
					}
				}
			}

			void emitConstants() {
				for (const auto& constant: model.constants) {
					if (!wanted(constant.location, constant.name)) continue;
					emitConstant(constant.name, constant.type, constant.value);
				}
			}
		};

	}

	DkModule lower(const CModel& model, const LowerOptions& options) {
		return Lowerer(model, options).run();
	}

}
