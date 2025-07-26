#pragma once
#include "utils.hpp"
#include "display_elements.hpp"

namespace dia_app {
namespace dia_file {
    
    struct CodeData {
        struct Location {
            std::string file;
            uint line, column;
            uint last_modified;

            Location() {}
            Location(const json &location) {
                ASSUME_HAS_STR_ASSIGN(location, file);
                ASSUME_HAS_UINT_ASSIGN(location, line);
                ASSUME_HAS_UINT_ASSIGN(location, column);
                ASSUME_HAS_UINT_ASSIGN(location, last_modified);
            }
        };
        Location location;
        Ptr content;

        CodeData() {}
        CodeData(const json &data) {
            ASSUME_HAS(data, "location");
            location = Location(data["location"]);
            
            ASSUME_HAS(data, "content");
            content = parse(data["content"]);
        }
    };

    // Message parameter data (message template identifier and params)
    struct ParamData {
        ShortMetadata metadata;
        std::map<std::string, Ptr> params;
        std::optional<CodeData> code;
        std::vector<ExploreEdgeParams> explore_edges;
        
        ParamData() {}
        ParamData(const json &param_data) {
            ASSUME_HAS(param_data, "metadata");
            metadata = ShortMetadata(param_data["metadata"]);
            
            ASSUME_HAS(param_data, "params");
            ASSUME_OBJ(param_data["params"]);
            for (auto &[key, val] : param_data["params"].items()) {
                params[key] = parse(val);
            }

            if (param_data.contains("code")) {
                code = CodeData(param_data["code"]);
            }

            if (param_data.contains("explore_edges")) {
                for (auto &edge : param_data["explore_edges"]) {
                    explore_edges.emplace_back(edge);
                }
            }
        }

        ParamData(const ShortMetadata &metadata) : metadata(metadata) {}
    };

} // namespace dia_file
} // namespace dia_app