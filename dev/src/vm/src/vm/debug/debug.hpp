#include <iostream>
#include <base/ints.hpp>

namespace vm {
    class DebugTracer {
    public:
        static void setDebug(bool enabled) { debug_enabled = enabled; }
        static bool isDebug() { return debug_enabled; }

        DebugTracer(const char* func)
            : func_name(func) {
            if (!debug_enabled) return;
            std::cerr << std::string(depth * 2, ' ')
                    << ">> Enter " << func_name << "\n";
            ++depth;
        }

        ~DebugTracer() {
            if (!debug_enabled) return;
            --depth;
            std::cerr << std::string(depth * 2, ' ')
                    << "<< Exit  " << func_name << "\n";
        }

    private:
        std::string func_name;
        inline static thread_local u64 depth = 0;
        inline static bool debug_enabled = false;
    };
}
#define TRACE_FUNC() DebugTracer tracer(__FUNCTION__)