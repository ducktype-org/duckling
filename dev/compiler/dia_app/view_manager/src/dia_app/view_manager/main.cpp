#include <bits/stdc++.h>
// #include <json/json.hpp>

using namespace std;

// using nlohmann::json;
// using uuid_t = int64_t;

// class Component {
//     private:
//     uuid_t uuid;

//     public:
//     Component(uuid_t uuid) : uuid(uuid) {}

//     virtual ~Component() {}
// };

// class TextComponent;
// class ConcatComponent;
// class InteractiveComponent;

// const string TEXT_COMPONENT_ANNOTATION = "text";
// const string CONCAT_COMPONENT_ANNOTATION = "concat";
// const string INTERACTIVE_COMPONENT_ANNOTATION = "interactive";

// using text_comp_ptr = shared_ptr<TextComponent>;
// using concat_comp_ptr = shared_ptr<ConcatComponent>;
// using inter_comp_ptr = shared_ptr<InteractiveComponent>;
// using comp_ptr = variant<text_comp_ptr, concat_comp_ptr, inter_comp_ptr>;

// class TextComponent : Component {
//     private:
//     string content;

//     TextComponent(uuid_t uuid, string content) : Component(uuid), content(std::move(content)) {}

//     public:
//     static unique_ptr<TextComponent> fromJson(const json &input) {
//         uuid_t uuid = input["uuid"];
//         string content = input["content"];
//         return make_unique<TextComponent>(TextComponent(uuid, content));
//     }
// };


// class ConcatComponent : Component {
//     private:
//     vector<comp_ptr> sub_components;

//     public:
//     static unique_ptr<ConcatComponent> from_json(const json &input) {
        
//     }
// };

// class InteractiveComponent : Component {
//     private:
//     vector<comp_ptr> closed;
//     vector<comp_ptr> expanded;

//     public:
//     static unique_ptr<InteractiveComponent> from_json(const json &input) {
        
//     }
// };

// // class EntryComponent : Component {

// // };

// class ViewManager {
//     private:
//     unordered_map<uuid_t, comp_ptr> uuid_to_component;

//     ViewManager() {}

//     static comp_ptr parse_component(const json& input) {
//         if (input["type"] == TEXT_COMPONENT_ANNOTATION) {
//             return text_comp_ptr(TextComponent::fromJson(input));
//         }
//         else if (input["type"] == CONCAT_COMPONENT_ANNOTATION) {
//             return concat_comp_ptr(ConcatComponent::from_json(input));
//         }
//         else if (input["type"] == INTERACTIVE_COMPONENT_ANNOTATION) {
//             return inter_comp_ptr(InteractiveComponent::from_json(input));
//         }
//         else {
//             throw "Unsupported component type!";
//         }
//     }

//     static vector<comp_ptr> parse_components(const json::array_t& input) {
//         vector<comp_ptr> result;
//         transform(input.begin(), input.end(), result.begin(), [](const json& comp_json) {
//             return parse_component(comp_json);
//         });
//         return result;
//     }

//     public:
//     static unique_ptr<ViewManager> create_from_json(json input) {
//         auto components = parse_components(input["components"]);

//         auto parse_errors = [](json input) {

//         };
//         return unique_ptr<ViewManager>();
//     }

//     void register_interaction(uuid_t uuid) {

//     }

//     json dump() {

//     }
// };

int main(int argc, char* argv[]) {
    // if (argc != 2) {
    //     std::cerr << "Pass a single file as argument" << std::endl;
    //     return 1;
    // }
    
    // std::ifstream file(argv[1]);
    // json input = json::parse(file);
    // file.close();

    // ViewManager view_manager = ViewManager::create_from_json(input);

    // stawić serwis

    cout << "test" << endl;
    
    return 0;
}