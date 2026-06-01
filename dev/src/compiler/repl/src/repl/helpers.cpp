#include "helpers.hpp"

namespace compiler::repl {
	void printReplCommandsHelp(std::ostream& out) {
		out << "\n=== REPL Commands ===\n";
		out << "  /help, /?, /h                   - Show this help message\n";
		out << "  /exit, /quit, /q                - Exit the REPL\n";
		out << "  /reset [n|-n]                   - Restart the REPL process to the state number "
			   "n|to the state n entries ago(for this option, need to provide - before n). Without "
			   "providing any number, it will reset to the clean starting state.";
		out << "  /history, /hist                 - Show session history (all statements executed "
			   "in this repl session)\n";
		out << "  /commands, /cmds                - Show all input history (editor history)\n";
		out << "  /commands-reset, /cmds-reset    - Clear input history (editor history)\n";
		out << "  /clear, /c                      - Clear terminal\n";
		out << "  /load <file.ds>                 - Load script file (stops on first error; "
			   "previous statements stay applied)\n";
	}
}  // namespace compiler::repl
