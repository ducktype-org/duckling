#include "components.hpp"

#include <proto/view.pb.h>

Component::~Component() {}

shared_ptr<Component> Component::deepCopy() {
    return {};
}

::view::Component Component::getView() const {
    return {};
}

Section::~Section() {}

::view::Section Section::getView() const {
    return {};
}