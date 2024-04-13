add_library(Diagnostic
        src/diagnostic/source_position.cpp
        src/diagnostic/logger.cpp
        src/diagnostic/message.cpp
)

make_module(Diagnostic USES Printer Filesystem)
