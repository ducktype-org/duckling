#include "cli_options.hpp"

clap::Clap standardOptions() {
    return clap::Clap()
        .addHelpFlag()
        .add(clap::ParamBuilder::ofFlag()
                    .addLongName("logger-cerr")
                    .addShortDesc(
                        "If set, Logger class will immediately print its messages to cerr. "
                        "Useful for debugging."
                    )
                    .build())
        .add(clap::ParamBuilder::ofFlag()
                    .addLongName("lexer-cerr")
                    .addShortDesc(
                        "If set, Lexer class will immediately print parsed tokens to cerr. "
                        "Useful for debugging."
                    )
                    .build());
}