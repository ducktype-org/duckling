#pragma once
#include "utils.hpp"
#include "display_elements.hpp"

namespace dia_app {
namespace dia_file {
    
    struct CodeData {
        struct Location {
            std::string file;
            u32 line, column;
            u32 last_modified;

            Location() {}
            Location(const json &location) {
                ASSUME_HAS_STR_ASSIGN(location, file);
                ASSUME_HAS_UINT_ASSIGN(location, line);
                ASSUME_HAS_UINT_ASSIGN(location, column);
                ASSUME_HAS_UINT_ASSIGN(location, last_modified);
            }
        };
        Location location;
        DisplayPtr content;

        CodeData() {}
        CodeData(const json &data) {
            ASSUME_HAS(data, "location");
            location = Location(data["location"]);
            
            ASSUME_HAS(data, "content");
            content = parse(data["content"]);
        }
    };

    // Message parameter data (message template identifier and params)
    struct InfoParams {
        ShortMetadata metadata;
        base::HashMap<std::string, DisplayPtr> params;
        base::Optional<CodeData> code;
        std::vector<ExploreEdgeParams> explore_edges;
        
        InfoParams() {}
        InfoParams(const json &info) {
            // Parse required meta
            ASSUME_HAS(info, "metadata");
            metadata = ShortMetadata(info["metadata"]);
            
            // Parse required params.
            ASSUME_HAS(info, "params");
            ASSUME_OBJ(info["params"]);
            for (auto &[key, val] : info["params"].items()) {
                params.put(key, parse(val));
            }
            
            // Parse optional code section.
            if (info.contains("code")) {
                code = CodeData(info["code"]);
            }
            
            // Parse optional explore edges.
            if (info.contains("explore_edges")) {
                for (auto &edge : info["explore_edges"]) {
                    explore_edges.emplace_back(edge);
                }
            }
        }

        InfoParams(const ShortMetadata &metadata) : metadata(metadata) {}
    };

    struct Entity {
        std::string kind;
        std::vector<InfoID> assoc_infos;
        base::HashMap<std::string, json> fields;

        Entity(const json &entity) {
            ASSUME_OBJ(entity);
            ASSUME_HAS_STR_ASSIGN(entity, kind);
            
            // Parse associated infos.
            ASSUME_HAS(entity, "assoc_infos");
            ASSUME_ARR(entity, "assoc_infos");
            for (auto &el : entity["assoc_infos"]) {
                ASSUME(el.is_string(), "info ID in assoc_infos must be a string");
                assoc_infos.push_back(el);
            }

            // Parse other fields.
            for (auto &[key, val] : entity.items()) {
                if (key == "assoc_infos") continue;
                fields.put(key, val);
            }
        }
    };
} // namespace dia_file
} // namespace dia_app