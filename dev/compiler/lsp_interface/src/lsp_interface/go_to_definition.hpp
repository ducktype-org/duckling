/**
 * @file go_to_definition.hpp
 * @brief Go to definition definition
 */

#pragma once

#include <pst_parser/pst.hpp>
#include <base/ref.hpp>

#include <string>

namespace lsp {
    std::string findDefinitions(MCRef<pst::LangElement>);
}