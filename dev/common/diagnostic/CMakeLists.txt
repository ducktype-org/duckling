add_library(Diagnostic
        src/diagnostic/source_position.cpp
        src/diagnostic/logger.cpp
        src/diagnostic/diagnostic_converters.cpp
        src/diagnostic/location.cpp
)

make_module(Diagnostic USES Printer Filesystem TokenFile)
