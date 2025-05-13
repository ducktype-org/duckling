#include <iostream>
#include <yaml-cpp/yaml.h>
#include "utils.hpp"
#include "template_elements.hpp"
#include "template_parser.hpp"

using namespace dia_app::message_template;

int main() {
    // Test parsing a scalar element
    YAML::Node scalar = YAML::Load("\"Sample text\"");
    auto elem1 = parse(scalar);

    return 0;
}
