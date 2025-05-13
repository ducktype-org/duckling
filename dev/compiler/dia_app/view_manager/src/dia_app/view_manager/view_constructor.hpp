#pragma once
#include "template_application.hpp"

namespace dia_app {
    
    struct ViewConstructor {
        using ParamData = dia_file::ParamData;

        // Main info section (error or warning).
        ParamData main_info;
        // List of secondary infos which can appear in the message.
        std::vector<ParamData> secondary_infos;
        // List of secondary infos displayed alongside the main info
        // in the specified order (others can be displayed after
        // interactions).
        std::vector<InfoHandle> displayed_secondary_infos;
        std::map<std::string, json> entities;

        // A collection of info handles to avoid duplicate info fetches.
        // 
        // Motivation: `InfoParamsHandle` has state - it is either
        //             an index into `secondary_infos` or a handle
        //             for an unfetched data. We need one designated
        //             place to reliably store that state without
        //             handle duplicates. That's why we have
        //             "stateless handles (`InfoHandle`) for stateful
        //             handles (`InfoParamsHandle`)".
        std::map<InfoHandle, InfoParamsHandle> info_handles;

        ViewConstructor(uint error_no, const json &data) {
            const json &info_group = data[error_no];

            main_info = ParamData(info_group["main_info"]);

            ASSUME_HAS(info_group, "secondary_infos");
            for (auto &info : info_group["secondary_infos"]) {
                secondary_infos.push_back(ParamData(info));
            }

            if (info_group.contains("displayed_secondary_infos")) {
                ASSUME_ARR(info_group, "displayed_secondary_infos");
                for (auto &el : info_group["displayed_secondary_infos"]) {
                    displayed_secondary_infos.push_back(InfoParamsHandle::add(el, data_handle()));
                }
            }

            ASSUME_OBJ(info_group["entities"]);
            entities = info_group["entities"];
        }

        std::expected<message_template::Info, message_template::Error> load_secondary_info(InfoHandle info_handle) {
            uint idx = InfoParamsHandle::load(info_handle, data_handle());
            ASSUME(idx < secondary_infos.size(), "secondary info index out-of-bounds");
            return load_info(secondary_infos[idx]);
        }

        std::expected<message_template::Info, message_template::Error> load_main_info() {
            return load_info(main_info);
        }

        DataHandle data_handle() {
            return DataHandle(entities, secondary_infos, info_handles);
        }
    private:
        std::expected<message_template::Info, message_template::Error> load_info(const ParamData &param_data) {
            return message_template::apply(param_data, data_handle());
        }
    };
}