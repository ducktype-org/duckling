#pragma once
#include "utils.hpp"
#include "display_elements.hpp"

namespace dia_app {
namespace dia_file {

    /**
     * @brief Parameters of an explore edge from the diagnostic file.
     * 
     * A part of `InfoParams`.
     * 
     */
    struct ExploreEdgeParams {
        std::string name;
        base::HashMap<std::string, DisplayPtr> params;
        InfoID info_id;

        ExploreEdgeParams(const json &edge);
    };
    
    /**
     * @brief Info's code section data from the diagnostic file.
     * 
     * A part of `InfoParams`.
     * 
     */
    struct CodeData {
        /**
         * @brief Code fragment's location.
         * 
         */
        struct Location {
            std::string file;
            u32 line, column;

            Location();
            Location(const json &location);
        };
        Location location;
        DisplayPtr content;

        CodeData();
        CodeData(const json &data);
    };

    /**
     * @brief Parametrization of an info instance from the diagnostic file.
     * 
     * Can be used to generate an `Info` instance after applying these
     * parameters to the appropriate info template.
     * 
     */
    struct InfoParams {
        // Metadata identifying the corresponding info template.
        ShortMetadata metadata;
        // The collection of parameters to be passed down to the info template.
        base::HashMap<std::string, DisplayPtr> params;
        // The optional code section of the info.
        base::Optional<CodeData> code;
        // The list of explore edge parameters to be passed down to
        // corresponding explore edge templates.
        std::vector<ExploreEdgeParams> explore_edges;
        
        InfoParams();
        // Construct `InfoParams` corresponding to a non-parametrized info
        // templated identified by `metadata`.
        InfoParams(const ShortMetadata &metadata);
        InfoParams(const json &info);
    };

    /**
     * @brief A piece of additional data shared across all info instances
     * within a given info group.
     * 
     */
    struct Entity {
        // The list of ids of infos associated with this entity.
        std::vector<InfoID> assoc_infos;
        // Additional fields which may be used for retrieving more
        // associated infos.
        base::HashMap<std::string, json> fields;

        Entity(const json &entity);
    };
} // namespace dia_file
} // namespace dia_app