#include "dia_parser.hpp"

namespace dia_app {
namespace dia_file {

	ExploreEdgeParams::ExploreEdgeParams(const json &edge)  {
		ASSUME_OBJ(edge);
		ASSUME_HAS_STR_ASSIGN(edge, name);
		ASSUME_HAS_UINT_ASSIGN(edge, handle);
		ASSUME_HAS(edge, "params");
		params = from_json<DisplayPtr>(edge["params"], [](const json &el) { return parse(el); });
	}

    CodeData::Location::Location() {}
    CodeData::Location::Location(const json &location) {
        ASSUME_HAS_STR_ASSIGN(location, file);
        ASSUME_HAS_UINT_ASSIGN(location, line);
        ASSUME_HAS_UINT_ASSIGN(location, column);
    }

    CodeData::CodeData() {}
    CodeData::CodeData(const json &data) {
        ASSUME_HAS(data, "location");
        location = Location(data["location"]);
        
        ASSUME_HAS(data, "content");
        content = parse(data["content"]);
    }

    InfoParams::InfoParams() {}
    InfoParams::InfoParams(const ShortMetadata &metadata) : metadata(metadata) {}
    InfoParams::InfoParams(const json &info) {
        // Parse required meta
        ASSUME_HAS(info, "metadata");
        metadata = ShortMetadata(info["metadata"]);
        
        // Parse required params.
        ASSUME_HAS(info, "params");
        params = from_json<DisplayPtr>(info["params"], [](const json &el) { return parse(el); });
        
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

    Entity::Entity(const json &entity) {
        ASSUME_OBJ(entity);
        
        // Parse associated infos.
        ASSUME_HAS(entity, "assoc_infos");
        ASSUME_ARR(entity, "assoc_infos");
        for (auto &el : entity["assoc_infos"]) {
            ASSUME(el.is_string(), "info ID in assoc_infos must be a string");
            assoc_infos.push_back(el);
        }

        // Parse other fields.
        fields = from_json<json>(entity);
        fields.erase("assoc_infos");
    }
}
}