#pragma once
#include "utils.hpp"
#include "display_elements.hpp"

namespace dia_app {
namespace dia_file {

    // An explore edge from the diagnostic file.
    struct ExploreEdgeParams {
        std::string name;
        base::HashMap<std::string, DisplayPtr> params;
        InfoID handle;

        ExploreEdgeParams(const json &edge);
    };
    
    struct CodeData {
        struct Location {
            std::string file;
            u32 line, column;
            u32 last_modified;

            Location();
            Location(const json &location);
        };
        Location location;
        DisplayPtr content;

        CodeData();
        CodeData(const json &data);
    };

    // Message parameter data (message template identifier and params)
    struct InfoParams {
        ShortMetadata metadata;
        base::HashMap<std::string, DisplayPtr> params;
        base::Optional<CodeData> code;
        std::vector<ExploreEdgeParams> explore_edges;
        
        InfoParams();
        InfoParams(const ShortMetadata &metadata);
        InfoParams(const json &info);
    };

    struct Entity {
        std::string kind;
        std::vector<InfoID> assoc_infos;
        base::HashMap<std::string, json> fields;

        Entity(const json &entity);
    };
} // namespace dia_file
} // namespace dia_app