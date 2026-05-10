#include <string>
#include <vector>

#ifdef _WIN32
	#include <process.h>
#else
    #include <unistd.h>
#endif

int execSelf(std::vector<std::string>& g_argv);