#pragma once

#include <filesystem/file.hpp>

#include "base/box.hpp"
#include "base/exceptions.hpp"
#include "base/ints.hpp"
#include "base/ref.hpp"
#include "base/stable_container.hpp"
#include "base/string_id.hpp"
#include "base/strongly_typed_id.hpp"

#include <exception>
#include <expected>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

/*
Idea:
    1. ArtMan really creates artifacts and contains all the data.
    2. Artifacts Manage themselves and it's a tree structure by default. It's a little better
because it maps better to the filesystem.

Variants vs inheritance:
    1. Variants are more flexible, but require more code?.
    2. Inheritance imposes a more concrete base
*/

namespace artman {
	// class ArtifactBase {
	// 	fs::FilePath path;

	// public:
	// 	ArtifactBase(fs::FilePath path): path(std::move(path)) {}

	// 	virtual ~ArtifactBase() = default;
	// 	/**
	// 	 * @brief Called by the ArtMan upon e.g. synchronization with filesystem.
	// 	 */
	// 	virtual std::expected<void, std::string> dump() = 0;
	// };

	// class BytesArtifact: public ArtifactBase {
	// public:
	// 	[[nodiscard]] virtual std::vector<byte> getBytes() const = 0;

	// 	std::expected<void, std::string> dump() override {
	// 		auto bytes = getBytes();
	// 		return {};
	// 	}
	// };

	// class ArtMan {
	// 	fs::FilePath artifacts_root;

	// 	ArtMan(fs::FilePath artifacts_root): artifacts_root(std::move(artifacts_root)) {}

	// public:
	//     std::expected<ArtMan, std::string> reader(fs::FilePath artifacts_root); // using fs_tree
	//     std::expected<ArtMan, std::string> writer(fs::FilePath artifacts_root);
	// };

	/**
	 * @brief A collection of bytes.
	 * @note It cannot be read-only, because `std::vector<const byte>` does not compile.
	 */
	using Bytes = std::vector<byte>;

	STRONG_TYPEDEF_ID_DIRECT_CREATION(ArtifactCollectionId);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(ArtifactInnerId);

	struct ArtifactId {
		const ArtifactCollectionId PARENT_ID;
		const ArtifactInnerId      INNER_ID;
	};

	class ArtifactCollection;

	class Artifact {
		const Ref<ArtifactCollection> PARENT;
		const ArtifactId              ID{};

	public:
		const Bytes BYTES;

		Artifact(Ref<ArtifactCollection> parent, ArtifactId id, Bytes bytes):
			  PARENT(parent),
			  ID(std::move(id)),
			  BYTES(std::move(bytes)) {}

		virtual ~Artifact() = default;

		/**
		 * @brief Called by the ArtMan upon e.g. synchronization with filesystem.
		 */
		virtual std::expected<void, std::string> flush() = 0;

		// @todo Add hashing of ID
	};

	enum class GroupingMode { Individual, Combined };

	/**
	 * @brief Represents an artifact group. (It basically maps to a directory.)
	 */
	class ArtifactCollection {
		static constexpr i64 PROTOCOL_VERSION = 1;

		/**
		 * @brief Represents how
		 */
		std::vector<Box<Artifact>>           sub_artifacts;
		std::vector<Box<ArtifactCollection>> sub_collections;

	public:
		const fs::FilePath                            PATH;
		const base::Optional<Ref<ArtifactCollection>> PARENT;

		const ArtifactCollectionId ID;

		const GroupingMode GROUPING_MODE = GroupingMode::Individual;

		ArtifactCollection(
			fs::FilePath                            root,
			GroupingMode                            grouping_mode,
			base::Optional<Ref<ArtifactCollection>> parent = {}
		):
			  PATH(std::move(root)),
			  PARENT(parent),
			  ID(0),
			  GROUPING_MODE(grouping_mode) {}

		ArtifactCollection(fs::FilePath root):
			  ArtifactCollection(std::move(root), GroupingMode::Individual) {}

		Ref<ArtifactCollection> newCollection(base::StrID name, GroupingMode grouping_mode) {
			auto new_path = PATH.createDirectoryIn(name.strView());
			sub_collections.push_back(makeBox<ArtifactCollection>(new_path, grouping_mode, this));
			return sub_collections.back().ref();
		}

		template<class T, class... Args>
		requires std::is_base_of_v<Artifact, T> Ref<T> newArtifact(Args&&... args) {
			sub_artifacts.push_back(base::makeBox<T>(
				this,
				ArtifactId(ID, ArtifactInnerId(sub_artifacts.size())),
				std::forward<Args>(args)...
			));
            return sub_artifacts.back().ref();
		}

		// ~ArtifactCollection() { flush(); }

		void flush() {
			if (PARENT) PARENT.value()->flush();
			if (FLUSH_MODE == FlushMode::Lazy) {
				// for (auto artif: artifacts) {
				// 	artif.dump();
				// }
			}
		}
	};
}
