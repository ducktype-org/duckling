#pragma once
#include <nlohmann/json_fwd.hpp>
#include "filesystem/file_path.hpp"
#include <string_id/string_id.hpp>


namespace compiler::driver {

    /**
     * @brief Describes a single external dependency of the main package.
     * This dependency info should be returned from parsing compilation options (e.g. from a manifest file).
     * And transferred to global_state::DependencyInfo in the process of compiler initialization.
	 */
	struct RawDependencyInfo final {
        /**
         * @brief The name of the dependency package.
         * This name must be unique and it's used as package_id in global_state::DependencyInfo.
         */
		base::StrID package_name;

        /**
         * @brief Alias of the dependency used in some package code.
         */
        base::StrID alias;

        static base::Optional<RawDependencyInfo> fromJson(const nlohmann::json& json);
	};


    struct RawPackageInfo final {
		base::StrID                 package_name;
        base::StrID                 version;
		fs::FilePath                package_path;
        std::vector<base::StrID>    features;
		std::vector<RawDependencyInfo> dependencies;

        static base::Optional<RawPackageInfo> fromJson(const nlohmann::json& json);
	};
}