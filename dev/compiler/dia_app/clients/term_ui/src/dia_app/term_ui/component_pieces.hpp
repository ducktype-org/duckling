#pragma once
#include <vector>
#include <set>
#include <proto/view.pb.h>

namespace term_ui {

    class TextPieces {
        std::vector<std::string> pieces;
    public:
        TextPieces(const view::NoHlComponent &component) {
            add_component(component);
        }

        std::string to_string() const {
            std::string res;
            for (auto &piece : pieces) {
                res += piece;
            }
            return res;
        }

    private:
        void add_component(const view::NoHlComponent &component) {
            // Associated entries are ignored in static UI.
            if (component.has_text_component()) {
                pieces.push_back(component.text_component().content());

            } else if (component.has_code_component()) {
                // Code is not distinguished from plain text in the terminal.
                pieces.push_back(component.code_component().content());

            } else if (component.has_interactive_component()) {
                add_component(component.interactive_component().primary_component());
                
            } else { // component.has_concat_component()
                auto &concat = component.concat_component();
                for (uint i = 0; i < concat.components_size(); ++i) {
                    add_component(concat.components(i));
                }
            }
        }
    };

    class CodePiece {
        std::string text;
        std::set<uint> groups;
    public:
        CodePiece(std::string text) : text(text) {}
        CodePiece(std::string text, std::set<uint> groups) : text(text), groups(groups) {}
    
        CodePiece(const view::HlCodeComponent &component) {
            text = component.content();
            for (uint i = 0; i < component.hl_tags_size(); ++i) {
                groups.insert(component.hl_tags(i));
            }
        }

        const std::set<uint> &getGroups() const {
            return groups;
        }

        const std::string &getText() const {
            return text;
        }

        std::string to_string() const {
            return text;
        }
    };

    class CodePieces {
        std::vector<CodePiece> pieces;
    public:
        CodePieces(const view::HlComponent &component) {
            add_component(component);
        }

        const std::vector<CodePiece> &getPieces() const {
            return pieces;
        }

        std::string to_string() const {
            std::string res;
            for (auto &piece : pieces) {
                res += piece.to_string();
            }
            return res;
        }

    private:
        void add_component(const view::HlComponent &component) {
            if (component.has_code_component()) {
                pieces.emplace_back(component.code_component());
            } else if (component.has_interactive_component()) {
                add_component(component.interactive_component().primary_component());
            } else { // component.has_concat_component()
                auto &concat = component.concat_component();
                for (uint i = 0; i < concat.components_size(); ++i) {
                    add_component(concat.components(i));
                }
            }
        }
    };
}