#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"
#include <bits/stdc++.h>
#include <json/json.hpp>

#include <grpcpp/grpcpp.h>
#include <memory>
#include <proto/view.pb.h>
#include <proto/view.grpc.pb.h>
#include <string>

using namespace std;

using nlohmann::json;
using uuid_t = int64_t;

class Component {
    private:
    uuid_t uuid;

    public:
    Component(uuid_t uuid) : uuid(uuid) {}

    virtual ~Component() {}
};

class TextComponent;
class ConcatComponent;
class InteractiveComponent;

const string TEXT_COMPONENT_ANNOTATION = "text";
const string CONCAT_COMPONENT_ANNOTATION = "concat";
const string INTERACTIVE_COMPONENT_ANNOTATION = "interactive";

using text_comp_ptr = shared_ptr<TextComponent>;
using concat_comp_ptr = shared_ptr<ConcatComponent>;
using inter_comp_ptr = shared_ptr<InteractiveComponent>;
using comp_ptr = variant<text_comp_ptr,
                         concat_comp_ptr,
                         inter_comp_ptr>;

using uuid_to_component_t = unordered_map<uuid_t, comp_ptr>;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;



class TextComponent : Component {
    private:
    string content;

    TextComponent(uuid_t uuid, string content) : Component(uuid), content(std::move(content)) {}

    public:
    static unique_ptr<TextComponent> fromJson(const json &input) {
        uuid_t uuid = input["uuid"];
        string content = input["content"];
        return make_unique<TextComponent>(TextComponent(uuid, content));
    }
};


class ConcatComponent : Component {
    private:

    public:
    vector<comp_ptr> sub_components;
    ConcatComponent(uuid_t uuid, vector<comp_ptr> components) : Component(uuid), sub_components(std::move(components)) {}

    static unique_ptr<ConcatComponent> fromJson(const json &input) {
        uuid_t uuid = input["uuid"];
        vector<comp_ptr> empty;
        return make_unique<ConcatComponent>(ConcatComponent(uuid, empty));
    }
};

class InteractiveComponent : Component {
    private:

    public:
    vector<comp_ptr> showed, hidden, ori_showed;

    InteractiveComponent(uuid_t uuid, vector<comp_ptr> showed, vector<comp_ptr> hidden, vector<comp_ptr> ori_showed) : Component(uuid), showed(std::move(showed)), hidden(std::move(hidden)), ori_showed(std::move(ori_showed)) {}

    static unique_ptr<InteractiveComponent> fromJson(const json &input) {
        uuid_t uuid = input["uuid"];
        vector<comp_ptr> empty;
        return make_unique<InteractiveComponent>(InteractiveComponent(uuid, empty, empty, empty));
    }
};

uuid_t getUuid() {
    static uuid_t cnt = 0;
    ++cnt;
    return cnt;
}

comp_ptr recursiveCopy(comp_ptr component);

vector<comp_ptr> copySubs(const vector<comp_ptr>& subs) {
    vector<comp_ptr> result;
    transform(subs.begin(), subs.end(), result.begin(), [](const comp_ptr& sub) {
        return recursiveCopy(sub);
    });
    return result;
};

comp_ptr recursiveCopy(comp_ptr component) {
    return std::visit(overloaded{
        [](text_comp_ptr comp){
            return comp_ptr(comp);
        },
        [](const concat_comp_ptr &comp){
            return comp_ptr(make_shared<ConcatComponent>(getUuid(), copySubs(comp->sub_components)));
        },
        [](const inter_comp_ptr& comp){
            return comp_ptr(make_shared<InteractiveComponent>(getUuid(), copySubs(comp->ori_showed), comp->hidden, comp->ori_showed));
        }
    }, component);
}

class ViewManager {
    private:
    uuid_to_component_t uuid_to_component;
    vector<comp_ptr> errors;

    ViewManager() {}

    static comp_ptr parseComponent(const json& input) {
        if (input["type"] == TEXT_COMPONENT_ANNOTATION) {
            return text_comp_ptr(TextComponent::fromJson(input));
        }
        else if (input["type"] == CONCAT_COMPONENT_ANNOTATION) {
            return concat_comp_ptr(ConcatComponent::fromJson(input));
        }
        else if (input["type"] == INTERACTIVE_COMPONENT_ANNOTATION) {
            return inter_comp_ptr(InteractiveComponent::fromJson(input));
        }
        else {
            throw "Unsupported component type!";
        }
    }

    static vector<comp_ptr> parseComponents(const json::array_t& input) {
        vector<comp_ptr> result;
        transform(input.begin(), input.end(), result.begin(), [](const json& comp_json) {
            return parseComponent(comp_json);
        });
        return result;
    }

    public:
    static unique_ptr<ViewManager> createFromJson(json input) {
        // unordered_set<uuid_t> processed;

        // function<comp_ptr(json)> build = [&](json &input) {

        // };
        // for (auto error : input["errors"]) {

        // }

        // map<uuid_t, vector<uuid_t>> graph;
        // map<uuid_t, size_t> deg;
        // vector<uuid_t> nodes;
        // queue<uuid_t> que;

        // for (auto component : input["components"]) {
        //     nodes.emplace_back(component["uuid"]);
        //     if (component["type"] == CONCAT_COMPONENT_ANNOTATION) {
        //         for (auto other : component["subcomponents"]) {
        //             ++deg[other];
        //             graph[component["uuid"]].emplace_back(other);
        //         }
        //     }
        //     else if (component["type"] == INTERACTIVE_COMPONENT_ANNOTATION) {
        //         for (auto other : component["closed"]) {
        //             ++deg[other];
        //             graph[component["uuid"]].emplace_back(other);
        //         }
        //         for (auto other : component["expanded"]) {
        //             ++deg[other];
        //             graph[component["uuid"]].emplace_back(other);
        //         }
        //     }
        // }
        // for (auto node : nodes) {
        //     if (deg[node] == 0) {
        //         que.emplace(node);
        //     }
        // }

        // while (!que.empty()) {

        // }


        // auto components = parseComponents(input["components"]);

        // auto parse_errors = [](json input) {

        // };
        return unique_ptr<ViewManager>();
    }

    void registerInteraction(uuid_t uuid) {
        comp_ptr component = this->uuid_to_component.at(uuid);
        std::visit(overloaded{
            [](const text_comp_ptr& ){},
            [](const concat_comp_ptr& ){},
            [](const inter_comp_ptr& comp){
                comp->showed = copySubs(comp->hidden);
                swap(comp->ori_showed, comp->hidden);
            }
        }, component);
    }

    // json dump() {

    // }
};

// class ViewServiceImpl : public view::ViewService::Service {
//     ::grpc::Status GetView(::grpc::ServerContext* context, const ::view::Empty* request, ::view::ViewResponse* response) override {
//         cerr << "Received GetView" << endl;
//         auto component = make_unique<::view::Component>();
//         component->set_id(5);
//         auto text_component = make_unique<::view::TextComponent>();
//         ::view::TextEntry text_entry1;
//         text_entry1.set_text("Pierwszy tekst.");
//         text_entry1.set_group_id(17);
//         text_entry1.set_type(::view::TextDisplayType::PLAIN);
//         ::view::TextEntry text_entry2;
//         text_entry2.set_text("Drugi tekst.");
//         text_entry2.set_group_id(17);
//         text_entry2.set_type(::view::TextDisplayType::PLAIN);
//         auto tmp = {text_entry1, text_entry2};
//         text_component->mutable_entries()->Add(tmp.begin(), tmp.end());
//         component->set_allocated_text_component(text_component.release());
//         response->set_allocated_component(component.release());
//         return ::grpc::Status::OK;
//     }

//     ::grpc::Status Click(::grpc::ServerContext* context, const ::view::ClickRequest* request, ::view::ClickResponse* response) override {
//         cerr << "Received Click" << endl;
//         int32_t component_id = request->object_id();
//         response->set_status("Status of the response of request with object_id: " + to_string(component_id));
//         return ::grpc::Status::OK;
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

    // ViewServiceImpl service;
    // grpc::ServerBuilder builder;
    // builder.AddListeningPort("localhost:50051", grpc::InsecureServerCredentials());
    // builder.RegisterService(&service);
    // unique_ptr<grpc::Server> server(builder.BuildAndStart());
    // cout << "ViewManager started on port 50051" << endl;
    // server->Wait();
    
    return 0;
}