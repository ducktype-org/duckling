#include <fstream>
#include "view_constructor.hpp"
#include <iostream>

int main() {
    using json = nlohmann::json;
    using ViewConstructor = dia_app::ViewConstructor;

    std::ifstream data_file("../../../../research/misje/07-serializacja/sample-2.json");
    json data = json::parse(data_file);
    ViewConstructor view_ctor(0, data);
    dia_app::DataHandle dh = view_ctor.data_handle();

    // Print part of main info in plain text.
    if (auto result = view_ctor.load_main_info(); result.has_value()) {
        std::cout << result.value().header_message->to_text(dh) << std::endl;
        std::cout << std::endl;
        std::cout << result.value().code.value().content->to_text(dh) << std::endl;
        std::cout << std::endl;
    } else {
        throw result.error();
    }

    // ---------------- LAZY INFO FETCHING ---------------- //
    {
        // Get the first secondary info's stateless handle.
        dia_app::InfoHandle info_handle = view_ctor.displayed_secondary_infos[0];
        // Load the chosen info and print its code on the screen.
        if (auto result = view_ctor.load_secondary_info(info_handle); result.has_value()) {
            std::cout << "info code:\n";
            std::cout << result.value().code.value().content->to_text(dh) << std::endl;
        } else {
            throw result.error();
        }
    }

    // ---------------- ASSOCIATED INFOS ---------------- //

    // Get an EntityElement passed to the first secondary info as a parameter.
    // Note: generally do not access secondary_infos with plain indices,
    //       instead use InfoHandle as the info may not have been fetched.
    auto entity = view_ctor.secondary_infos[0].params["alternative"];
    
    // Get the first info id associated with this entity (with `sample-2.json` will require additional "fetching").
    dia_app::InfoHandle info_handle = entity->get_assoc_infos(dh)[0];
    // Load the chosen info and print its part on the screen.
    if (auto result = view_ctor.load_secondary_info(info_handle); result.has_value()) {
        std::cout << "Associated info:\n";
        std::cout << result.value().header_message->to_text(dh) << std::endl;
    } else {
        throw result.error();
    }
}