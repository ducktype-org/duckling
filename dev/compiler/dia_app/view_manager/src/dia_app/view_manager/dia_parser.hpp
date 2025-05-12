#pragma once
#include "utils.hpp"
#include "display_elements.hpp"

namespace dia_app {
namespace dia_file {
    // ----- DIAGNOSTICS FILE SYNTAX -----

    // Display element types:
    // - (1st top-most) entity <- { "type": "entity", "refers_to": ... }
    // - (2nd) interact <- { ... "alt_content": ... }
    // - (3rd) concat, <- []
    // - (4th) lines, <- { "type": "lines", "content": ... }
    // - (5th) text, <- "some text" or { "type": "text", ... }

    // Co robi compiler:
    // - zbiera paramsy do templatek tekstowych
    // - robi rozkład na content i alt_content
    // - ustawia podkreślenia
    // - dodaje metadane o entities
    // - dodaje explore_edges

    // Co robi view constructor:
    // - parsuje plik .dia
    // - aplikuje templatki tekstowe (parsowanie templatki, substitution, dodanie nowych infos)
    // - skanuje metadane o entities w celu dodania assoc_infos
    // - zwraca repr wewn state managera

    // Co robi state manager:
    // - komunikuje się z UI
    // - formułuje zapytania do view constructora
    // - utrzymuje stan widoku

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
        std::vector<InfoHandle> explore_edges;
        
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
                    ASSUME(edge.is_number_unsigned(), "explore edge is not an unsigned integer");
                    explore_edges.push_back(edge);
                }
            }
        }

        ParamData(const ShortMetadata &metadata) : metadata(metadata) {}
    };

} // namespace dia_file
} // namespace dia_app