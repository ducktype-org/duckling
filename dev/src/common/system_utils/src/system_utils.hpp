#include <string>
#include <vector>

 /**
 * @brief Relaunches the current executable with the original arguments.
 *
 * On POSIX systems, this replaces the current process image.
 * On Windows systems, this spawns a new process and keeps the original one alive.
 *
 * @param g_argv The argv used to start the original process.
 * @return A -1 error code on failure. On POSIX, a successful exec call does not return.
 *         On Windows, returns the exit code of the spawned process.
 */
int execSelf(std::vector<std::string>& g_argv);
