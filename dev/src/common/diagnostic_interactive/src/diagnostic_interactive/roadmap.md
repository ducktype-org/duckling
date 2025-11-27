

class DiagnosticBase {
    std::vector<Params> params;
    std::vector<Entity> entities;
    std::vector<PointerMessage> pointer_messages;
    std::vector<ExploreLink> explore_links;

    std::vector<AutomaticInteractivity> automatic_interactivity;

    serialize () {

    }

    abstract getMetadata()

    abstract getInteractivity

}

protected:
    addExploreLink()

    addPointerMessage()

    addNote()
}

class DiagnosticWithCode {
    

    serialize () {

    }
}

SomethingError : DiagnosticBase {

}