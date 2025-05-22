#include "utils.hpp"
#include "template_parser.hpp"
#include "template_application.hpp"
#include "dia_parser.hpp"

namespace dia_app {
    std::string MESSAGE_TEMPLATE_PATH = "../compiler/dia_app/view_manager/src/dia_app/view_manager/templates/";

    DataHandle::DataHandle(
        std::map<std::string, json> &entities,
        std::vector<ParamData> &secondary_infos,
        std::map<InfoHandle, InfoParamsHandle> &info_handles
    ) :
    entities(entities),
    secondary_infos(secondary_infos),
    info_handles(info_handles) {}

    TemplateDataHandle::TemplateDataHandle(
        const TemplateData &template_data,
        const ParamData &param_data,
        DataHandle data_handle
    ) :
    template_data(template_data),
    param_data(param_data),
    entities(data_handle.entities),
    secondary_infos(data_handle.secondary_infos),
    info_handles(data_handle.info_handles) {}

    DataHandle TemplateDataHandle::to_data_handle() const {
        return DataHandle(entities, secondary_infos, info_handles);
    }

    InfoParamsHandle::InfoParamsHandle(const json &handle_json) {
        if (handle_json.is_number_unsigned()) {
            idx = handle_json;
            return;
        }
        // Parsing a dummy handle - simply the param data
        // of the info. To be replaced by some LS-generated handle
        // for later access.
        ASSUME_HAS_STR(handle_json, "type");
        ASSUME_VAL(handle_json, "type", "dummy_handle");
        ASSUME_HAS(handle_json, "handle");
        param_data = handle_json["handle"];
    }

    uint InfoParamsHandle::load(DataHandle dh) {
        if (idx.has_value()) {
            return idx.value();
        }
        if (metadata.has_value()) {
            // Add a new info given metadata.
            dh.secondary_infos.push_back(dia_file::ParamData(metadata.value()));
            // Update the handle.
            idx = dh.secondary_infos.size() - 1;
            metadata.reset();
            // Return the new info's index.
            return idx.value();
        }
        // Otherwise it is a dummy handle.
        dh.secondary_infos.push_back(param_data.value());
        
        idx = dh.secondary_infos.size() - 1;
        param_data.reset();
        return idx.value();
    }

    uint InfoParamsHandle::load(InfoHandle info, DataHandle dh) {
        return dh.info_handles.at(info).load(dh);
    }

    InfoHandle InfoParamsHandle::add(const InfoParamsHandle &params_handle, DataHandle dh) {
        // Check if the handle already exists.
        for (auto &[key, val] : dh.info_handles) {
            if (val == params_handle) {
                return key;
            }
        }
        // If not, create a new one and return a stateless handle to it.
        InfoHandle handle = dh.info_handles.size();
        dh.info_handles[handle] = params_handle;
        return handle;
    }
}