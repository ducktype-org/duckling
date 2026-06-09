import os
import re

from ipykernel.kernelbase import Kernel
import pexpect


class MyLanguageKernel(Kernel):
    implementation = "IPython"
    implementation_version = "0.0.1"
    language = "Duckling"
    language_version = "0.0.1"
    banner = "Duckling Kernel"

    language_info = {
        "name": "duckling",
        "mimetype": "text/x-duckling",
        "file_extension": "duckling",
    }

    INPUT_PROMPT = "duckling> "
    OUTPUT_PROMPT = "=>"
    DEBUG_PROMPT = "[DEBUG]"
    EXPRESSION_COMPILED_PROMPT = "[Expression compiled]"

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        bin_path = "REPLACE_THIS_WITH_YOUR_PATH_TO_THIS/dev/build/bin"

        exec_name = os.path.join(bin_path, "duckc")

        args = ["repl"]

        if not os.path.exists(exec_name):
            raise FileNotFoundError(f"Haven't found duckc binary: {exec_name}")

        self.child = pexpect.spawn(exec_name, args, cwd=bin_path, encoding="utf-8")
        # Log all child output directly to the Jupyter console's stdout
        # self.child.logfile = sys.__stdout__

        try:
            self.child.expect(self.INPUT_PROMPT, timeout=5)
        except pexpect.TIMEOUT:
            self.log.warning("Expected to receive input prompt from REPL!\n")

    def do_execute(
        self, code, silent, store_history=True, user_expressions=None, allow_stdin=False
    ):
        if not code.strip():
            return {
                "status": "ok",
                "execution_count": self.execution_count,
                "payload": [],
                "user_expressions": {},
            }

        # Repl frontends now expects Alt+Enter to enter new line so we translate every \n into code of Alt+Enter.
        code_for_repl = code.replace("\n", "\x1b\r")

        # And we are adding byte to indicate to repl that we want to commit input.
        code_for_repl += "\r"

        self.child.send(code_for_repl)

        try:
            self.child.expect(self.INPUT_PROMPT, timeout=None)
        except pexpect.EOF:
            return {
                "status": "error",
                "ename": "ProcessKilled",
                "evalue": "Interpreter process died",
                "traceback": [],
            }

        # Text received before the prompt.
        raw_output = self.child.before

        lines = raw_output.splitlines()

        stdout_lines = []  # Here will land [DEBUG], [Expression compiled...] etc.
        result_text = None  # Intended output result - everything after "=>".

        found_result = False
        # self.log.warning(f"lines received: {lines}\n")
        # self.log.warning(f"=======================\n")

        for line in lines:
            if line.startswith(self.OUTPUT_PROMPT):
                result_text = line[2:].strip()  # Deletes '=>' and whitespaces.
                found_result = True
            elif found_result:
                result_text += "\n" + line.strip()
            elif line.startswith(self.DEBUG_PROMPT):
                stdout_lines.append(line)
            elif line.startswith(self.EXPRESSION_COMPILED_PROMPT):
                stdout_lines.append(line)
            else:
                continue

        if not silent:
            if stdout_lines:
                full_log = "\n".join(stdout_lines)
                stream_content = {"name": "stdout", "text": full_log}
                self.send_response(self.iopub_socket, "stream", stream_content)

            if result_text:
                content = {
                    "execution_count": self.execution_count,
                    "data": {"text/plain": result_text},
                    "metadata": {},
                }
                self.send_response(self.iopub_socket, "execute_result", content)

        return {
            "status": "ok",
            "execution_count": self.execution_count,
            "payload": [],
            "user_expressions": {},
        }
    
    # This method is called by Jupyter when user tries to use autocompletion.
    def do_complete(self, code, cursor_pos):
        text_until_cursor = code[:cursor_pos]
        match = re.search(r"\w+$", text_until_cursor)
        prefix = match.group(0) if match else ""

        if not prefix:
            return {
                "matches": [],
                "cursor_start": cursor_pos,
                "cursor_end": cursor_pos,
                "metadata": {},
                "status": "ok",
            }

        try:
            self.child.read_nonblocking(size=8192, timeout=0)
        except Exception:
            pass

        # We utilize custom command /complete in REPL to get list of possible completions for given prefix.
        self.child.send(f"/complete {prefix}\r")

        try:
            self.child.expect(self.INPUT_PROMPT, timeout=None)
        except Exception:
            pass

        raw_output = self.child.before or ""

        # We delete ANSI escape codes from the output added by replxx.
        ansi_escape = re.compile(r'(?:\x1b\[|\x9b)[0-?]*[ -/]*[@-~]')
        clean_output = ansi_escape.sub('', raw_output)

        lines = clean_output.splitlines()
        matches = []

        for line in lines:
            clean_line = line.strip()

            if not clean_line:
                continue
            if "/complete" in clean_line:
                continue
            if "End_of_completions" in clean_line:
                continue
            if "available" in clean_line:
                continue

            if clean_line not in matches:
                matches.append(clean_line)

        return {
            "matches": matches,
            "cursor_start": cursor_pos - len(prefix),
            "cursor_end": cursor_pos,
            "metadata": {},
            "status": "ok",
        }

    def do_shutdown(self, restart):
        self.child.close()
        return super().do_shutdown(restart)


if __name__ == "__main__":
    from ipykernel.kernelapp import IPKernelApp

    IPKernelApp.launch_instance(kernel_class=MyLanguageKernel)
