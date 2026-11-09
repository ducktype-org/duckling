// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "file_pst_inputs.hpp"

#include "collect_input.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <global_state/packages.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/types/bit256.hpp>

#include <logger/logger.hpp>

#include <atomic>
#include <cstring>
#include <functional>

namespace compiler::driver {

	namespace {
		struct FileRecord final {
			base::Bit256              content_hash;
			std::vector<base::Bit256> group_hashes;
		};

		/**
		 * @brief Records of the previous compilation, keyed by the component hash of the file.
		 * \parallel Written only by loadPreviousFilePSTInputs (before any query), read-only later.
		 */
		base::HashMap<base::Bit256, FileRecord> previous_records;

		/**
		 * @brief "DKPSTIN1": format magic and version of the serialized records.
		 */
		constexpr u64 FORMAT_MAGIC = 0x31'4e'49'54'53'50'4b'44ULL;

		[[nodiscard]]
		base::Bit256 fileKey(Ref<frontend::SourceFile> source_file) {
			return source_file->getComponentHash().hash;
		}

		/**
		 * @brief The previous record of the file if its content did not change.
		 */
		[[nodiscard]]
		base::Optional<CRef<FileRecord>> matchingPreviousRecord(Ref<frontend::SourceFile> source_file
		) {
			if (previous_records.empty()) return {};
			auto record = previous_records.atMaybe(fileKey(source_file));
			if (!record.has_value()) return {};
			if (record.value()->content_hash != source_file->getContentHash()) return {};
			return CRef<FileRecord>(record.value());
		}

		/**
		 * @brief Calls fun for every source file of the global packages (sequentially).
		 */
		void forEachSourceFile(const std::function<void(Ref<frontend::SourceFile>)>& fun) {
			auto visit = [&](this auto&& self, frontend::ModuleID module_id) -> void {
				auto module_ref = frontend::getModuleRef(module_id);
				if (module_ref->hasMainSourceFile())
					fun(frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
						module_ref->getMainSourceFile().illegalAccess().getID()
					));
				for (const auto& submodule: module_ref->getSubmodules().illegalAccess())
					self(submodule.illegalAccess().getID());
			};
			for (const auto& package: global_state::getPackages())
				visit(package.getRootModule().illegalAccess().getID());
		}

		void appendU64(std::vector<byte>& out, u64 value) {
			const auto* ptr = reinterpret_cast<const byte*>(&value);
			out.insert(out.end(), ptr, ptr + sizeof(value));
		}

		void appendBit256(std::vector<byte>& out, const base::Bit256& value) {
			for (u64 word: value.data) appendU64(out, word);
		}

		/**
		 * @brief Minimal bounds-checked reader of the serialized records.
		 */
		struct Reader final {
			std::span<const byte> data;
			usize                 pos = 0;

			bool readU64(u64& value) {
				if (data.size() - pos < sizeof(u64)) return false;
				std::memcpy(&value, data.data() + pos, sizeof(u64));
				pos += sizeof(u64);
				return true;
			}

			bool readBit256(base::Bit256& value) {
				for (u64& word: value.data)
					if (!readU64(word)) return false;
				return true;
			}
		};
	}

	void loadPreviousFilePSTInputs(std::span<const byte> data) {
		previous_records.clear();

		Reader reader{ .data = data };
		u64    magic = 0, count = 0;
		bool   ok = reader.readU64(magic) && magic == FORMAT_MAGIC && reader.readU64(count);

		base::HashMap<base::Bit256, FileRecord> records;
		for (u64 i = 0; ok && i < count; i++) {
			base::Bit256 key;
			FileRecord   record;
			u64          hash_count = 0;
			ok = reader.readBit256(key) && reader.readBit256(record.content_hash)
			  && reader.readU64(hash_count)
			  && hash_count <= (data.size() - reader.pos) / (4 * sizeof(u64));
			if (!ok) break;
			record.group_hashes.resize(hash_count);
			for (auto& hash: record.group_hashes) ok = ok && reader.readBit256(hash);
			if (ok) records.put(key, std::move(record));
		}

		if (!ok || reader.pos != data.size()) {
			CORE_DEV_LOG(Incremental, "Ignoring malformed file PST input records\n");
			return;
		}
		previous_records = std::move(records);
		CORE_DEV_LOG(
			Incremental, "Loaded file PST input records of ", previous_records.size(), " files\n"
		);
	}

	void prepareFilesForInputCollection() {
		std::atomic<u64> reused = 0, parsed = 0;
		for (const auto& package: global_state::getPackages()) {
			frontend::forEachSourceFileInParallel(
				package.getRootModule().illegalAccess().getID(),
				[&](Ref<frontend::SourceFile> source_file) {
					if (source_file->hasPST()) return;
					if (matchingPreviousRecord(source_file).has_value()) {
						reused.fetch_add(1, std::memory_order_relaxed);
						return;
					}
					source_file->getPST();
					parsed.fetch_add(1, std::memory_order_relaxed);
				}
			);
		}
		CORE_DEV_LOG(
			Incremental,
			"Input collection: ",
			reused.load(),
			" unchanged files not parsed, ",
			parsed.load(),
			" files parsed\n"
		);
	}

	void collectFilePSTInputs(
		Ref<frontend::SourceFile> source_file, std::vector<query::external::InputData>& out
	) {
		if (!source_file->hasPST()) {
			if (auto record = matchingPreviousRecord(source_file)) {
				for (const auto& hash: record.value()->group_hashes)
					out.emplace_back(pst::internal::PSTAccessSideInput::getID(), hash);
				return;
			}
		}
		collectQueryInputsFromPst(source_file->getPST(), out);
	}

	std::vector<byte> serializeFilePSTInputs() {
		struct Entry final {
			base::Bit256              key;
			base::Bit256              content_hash;
			std::vector<base::Bit256> group_hashes;
		};

		std::vector<Entry> entries;

		forEachSourceFile([&](Ref<frontend::SourceFile> source_file) {
			if (source_file->hasPST()) {
				entries.push_back(Entry{
					.key          = fileKey(source_file),
					.content_hash = source_file->getContentHash(),
					.group_hashes = source_file->getPST()->getUnlockGroupHashes(),
				});
			} else if (auto record = matchingPreviousRecord(source_file)) {
				entries.push_back(Entry{
					.key          = fileKey(source_file),
					.content_hash = record.value()->content_hash,
					.group_hashes = record.value()->group_hashes,
				});
			}
			// Otherwise the file was neither parsed nor known, it will be parsed next time.
		});

		std::vector<byte> out;
		appendU64(out, FORMAT_MAGIC);
		appendU64(out, entries.size());
		for (const auto& entry: entries) {
			appendBit256(out, entry.key);
			appendBit256(out, entry.content_hash);
			appendU64(out, entry.group_hashes.size());
			for (const auto& hash: entry.group_hashes) appendBit256(out, hash);
		}
		return out;
	}

	void parseAllUnparsedFiles() {
		std::atomic<u64> parsed = 0;
		forEachSourceFile([&](Ref<frontend::SourceFile> source_file) {
			if (!source_file->hasPST()) {
				source_file->getPST();
				parsed.fetch_add(1, std::memory_order_relaxed);
			}
		});
		CORE_DEV_LOG(
			Incremental, "Unknown stable hash: parsed ", parsed.load(), " not yet parsed files\n"
		);
	}
}
