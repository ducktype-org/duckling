#pragma once

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

namespace vm::loader::compiler {

	namespace detail {
		class LocalStackDbBuilder;
	};

	class LocalStackDatabase {
		friend detail::LocalStackDbBuilder;

	public:
		struct Entry {
			base::StrID var_name;
			TypeRef     type;
			usize       offset = 0;
		};

	private:
		struct Lifetime {
			usize deinit_idx                         = 0;
			usize init_idx                           = 0;
			auto  operator<=>(const Lifetime&) const = default;
		};

		struct DatabaseEntry {
			Entry    entry;
			Lifetime lifetime;
			usize    prev = 0;
		};

		base::HashMap<base::StrID, std::map<Lifetime, usize>> name_lifetime_bind;
		base::HashMap<usize, DatabaseEntry>                   database;
		usize                                                 max_size;
		usize                                                 stack_state = 0;

		LocalStackDatabase(
			decltype(name_lifetime_bind) lifetimes, decltype(database) db, usize max_size
		):
			  name_lifetime_bind(std::move(lifetimes)),
			  database(std::move(db)),
			  max_size(max_size) {}

	public:
		LocalStackDatabase() = default;

		void changeState(usize new_state) { stack_state = new_state; }

		std::vector<Entry> getFullStack() const {
			if (!database.contains(stack_state)) return {};

			std::vector<Entry> res              = {};
			usize              curr_stack_state = stack_state;

			do {
				auto& el = **database.atMaybe(curr_stack_state);
				res.push_back(el.entry);
				curr_stack_state = el.prev;
			} while (curr_stack_state != 0);

			return res;
		}

		base::Optional<Entry> atMaybe(const base::StrID var_name) const {
			if (!database.contains(stack_state)) [[unlikely]]
				return std::nullopt;

			if (!name_lifetime_bind.contains(var_name)) [[unlikely]]
				return std::nullopt;

			auto& lifetime     = (*database.atMaybe(stack_state))->lifetime;
			auto& varname_info = name_lifetime_bind.at(var_name);
			auto  it           = varname_info.lower_bound(lifetime);

			if (it == varname_info.end()) [[unlikely]]
				return std::nullopt;

			auto& found_lifetime = it->first;

			if (found_lifetime.init_idx > lifetime.deinit_idx
			    || found_lifetime.deinit_idx < lifetime.init_idx) [[unlikely]] {
				return std::nullopt;
			}

			usize found_stack_state = it->second;

			return (*database.atMaybe(found_stack_state))->entry;
		}

		usize maxSize() { return max_size; }
	};
}

namespace vm::loader::compiler::detail {

	class LocalStackDbBuilder {
		using Prod    = LocalStackDatabase;
		usize id      = 0;
		usize next_id = 1;

	public:
		LocalStackDbBuilder(base::StrID root_name, TypeRef root_type) {
			states.put(
				0,
				BuilderEntry{
					.entry = Prod::Entry{ .var_name = root_name, .type = root_type, .offset = 0 },
					.next  = {},
					.stack_idx = 0,
					.prev_idx  = 0,
				}
			);
		}

		void changeState(usize new_state) {
			CORE_ASSERT(states.contains(new_state), "Trying to use non-existant state");
			id = new_state;
		}

		usize push(base::StrID var_name, TypeRef var_type) {
			auto desc = std::make_pair(var_name, var_type);

			if (states[id].next.contains(desc)) {
				id = states[id].next[desc];
				return id;
			}

			states[id].next.put(desc, next_id);
			states.put(
				next_id,
				BuilderEntry{
					.entry     = Prod::Entry{ .var_name = var_name, .type = var_type, .offset = 0 },
					.next      = {},
					.stack_idx = next_id,
					.prev_idx  = id,
				}
			);

			id = next_id;
			next_id++;

			return id;
		}

		usize pop() {
			CORE_ASSERT(id != 0, "Tried to pop the root");
			auto new_id = states[id].prev_idx;
			id          = new_id;

			return id;
		}

		Prod finishBuilding(usize offset = 0) {
			base::HashMap<usize, Prod::Lifetime> lifetime;
			states[0].entry.offset = offset;

			if (states[0].entry.type->getName() != "void")
				offset += states[0].entry.type->getSize().asInt();

			usize order    = 0;
			usize max_size = offset;
			auto  dfs      = [&](auto&& self, usize node, usize curr_offset) -> void {
                lifetime.put(node, Prod::Lifetime{});
                lifetime[node].init_idx = order;
                order++;
                max_size = std::max(max_size, offset);

                auto& neighs = states[node].next;
                for (auto& [_, val]: neighs) states[val].entry.offset = curr_offset;

                for (auto& [key, val]: neighs)
                    self(self, val, curr_offset + key.second->getSize().asInt());

                lifetime[node].deinit_idx = order++;
			};

			dfs(dfs, 0, offset);

			decltype(Prod::name_lifetime_bind) lifetime_mangling{};
			decltype(Prod::database)           db;

			for (auto& [id, bd_entry]: states) {
				auto& var_name     = states[id].entry.var_name;
				auto& var_lifetime = lifetime[id];

				lifetime_mangling.put(var_name, {});

				lifetime_mangling[var_name].emplace(var_lifetime, id);

				db.put(
					id,
					Prod::DatabaseEntry{
						.entry    = states[id].entry,
						.lifetime = var_lifetime,
						.prev     = states[id].prev_idx,
					}
				);
			}

			return Prod{ lifetime_mangling, db, max_size };
		}

	private:
		struct nameTypeHasher {
			static std::size_t operator()(const std::pair<base::StrID, TypeRef>& val) {
				return std::hash<base::StrID>{}(val.first)
				     + std::hash<base::StrID>{}(val.second->getName());
			}
		};

		struct BuilderEntry {
			Prod::Entry                                                           entry;
			base::HashMap<std::pair<base::StrID, TypeRef>, usize, nameTypeHasher> next;
			usize                                                                 stack_idx;
			usize                                                                 prev_idx;
		};

		base::HashMap<usize, BuilderEntry> states;
	};
}
