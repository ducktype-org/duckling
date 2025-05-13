#include "components.hpp"

#include <proto/view.pb.h>

::view::Component Component::getView() const {
    return {};
}

Section::~Section() {}

::view::Section Section::getView() const {
    return {};
}