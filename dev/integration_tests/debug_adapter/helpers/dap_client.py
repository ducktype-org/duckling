import os
import sys
import json
import subprocess

class DAPTestClient:
    def __init__(self, program_name: str):
        if len(sys.argv) < 2:
            sys.stderr.write("ERROR: Build directory path was not provided as an argument!\n")
            sys.exit(1)

        build_dir = sys.argv[1]
        vm_binary_path = os.path.join(build_dir, "bin", "VM")

        self.current_seq = 1
        self.program_name = program_name
        self.full_output_history = []

        self.process = subprocess.Popen(
            [vm_binary_path, "debug_adapter"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=False,  
            bufsize=0    
        )
    def start_session(self):
        self.send_initialize()
        self.send_configuration_done()
        self.send_launch()

    def send_request(self, command: str, arguments: dict = None) -> int:
        if arguments is None:
            arguments = {}

        payload = {
            "seq": self.current_seq,
            "type": "request",
            "command": command,
            "arguments": arguments
        }
        
        json_str = json.dumps(payload)
        payload_bytes = json_str.encode('utf-8')
        header_str = f"Content-Length: {len(payload_bytes)}\r\n\r\n"
        
        self.process.stdin.write(header_str.encode('ascii') + payload_bytes)
        self.process.stdin.flush()

        assigned_seq = self.current_seq
        self.current_seq += 1
        return assigned_seq

    def send_initialize(self) -> int:
        return self.send_request("initialize")

    def send_launch(self) -> int:
        return self.send_request("launch", {"program": self.program_name})

    def send_set_breakpoints(self, file_path: str, lines: list[int]) -> int:
        breakpoints_arg = [{"line": line} for line in lines]
        return self.send_request("setBreakpoints", {
            "source": {"path": file_path},
            "breakpoints": breakpoints_arg
        })

    def send_configuration_done(self) -> int:
        return self.send_request("configurationDone")

    def send_continue(self) -> int:
        return self.send_request("continue")
    
    def send_pause(self) -> int:
        return self.send_request("pause")

    def send_next(self) -> int:
        return self.send_request("next")
    
    def send_evaluate(self, number) -> int:
        return self.send_request("evaluate", {"expression": f"{number}", "context": "repl"})

    def read_message(self) -> dict or None:
        content_length = 0
        stdout = self.process.stdout

        while True:
            line = stdout.readline()
            if not line:
                return None

            if line.startswith(b"Content-Length:"):
                content_length = int(line.split(b":")[1].strip())

            if line in (b"\r\n", b"\n", b"") or line.strip() == b"":
                if content_length > 0:
                    break

        body_bytes = stdout.read(content_length)
        if not body_bytes:
            return None

        raw_str = body_bytes.decode('utf-8')
        self.full_output_history.append(raw_str)

        sys.stderr.write(raw_str + "\n")
        sys.stderr.flush()

        try:
            return json.loads(raw_str)
        except json.JSONDecodeError:
            return {"raw_text": raw_str}

    # --- SYNCHRONIZATION HELPERS (Abstracting while loops) ---

    def wait_for_text(self, text: str, error_context: str):
        """Blocks until the specified text appears ANYWHERE in the message."""
        while True:
            msg = self.read_message()
            if msg is None:
                self.fail_test(f"EOF while waiting for: {error_context}")

            if "raw_text" in msg and text in msg["raw_text"]:
                break

            if text in json.dumps(msg):
                break

    def wait_for_response(self, request_seq: int, error_context: str, expect_success: bool) -> dict:
        while True:
            msg = self.read_message()
            if msg is None:
                self.fail_test(f"EOF while waiting for response to seq {request_seq} ({error_context})")

            if msg.get("type") == "response" and msg.get("request_seq") == request_seq:
                if msg.get("success") is not expect_success:
                    self.fail_test(f"Request {request_seq} failed unexpectedly: {msg.get('message')}")
                return msg

    def wait_for_event(self, event_name: str, error_context: str) -> dict:
        """Blocks until a specific DAP event (e.g. 'terminated', 'initialized') is received."""
        while True:
            msg = self.read_message()
            if msg is None:
                self.fail_test(f"EOF while waiting for event '{event_name}' ({error_context})")

            if msg.get("type") == "event" and msg.get("event") == event_name:
                return msg

    def wait_for(self, responses: list = None, events: list = None, outputs: list = None):
        """
        Blocks until ALL specified conditions are met, regardless of their arrival order.
        
        :param responses: List of request_seq (ints) that must receive success=True.
        :param events: List of event names (strings) that must be emitted (e.g. "stopped").
        :param outputs: List of text snippets (strings) that must appear in the console output.
        """
        pending_responses = set(responses or [])
        pending_events = list(events or [])
        pending_outputs = list(outputs or [])

        while pending_responses or pending_events or pending_outputs:
            msg = self.read_message()
            if msg is None:
                missing = f"responses={pending_responses}, events={pending_events}, outputs={pending_outputs}"
                self.fail_test(f"EOF while waiting. Still missing: {missing}")

            if msg.get("type") == "response":
                req_seq = msg.get("request_seq")
                if req_seq in pending_responses:
                    if msg.get("success") is not True:
                        self.fail_test(f"Expected successful response for seq {req_seq}, but got failure: {msg.get('message')}")
                    pending_responses.remove(req_seq)

            if msg.get("type") == "event":
                event_name = msg.get("event")
                
                if event_name in pending_events:
                    pending_events.remove(event_name)

                if event_name == "output":
                    output_text = msg.get("body", {}).get("output", "")
                    
                    for expected_out in pending_outputs[:]:
                        if expected_out in output_text:
                            pending_outputs.remove(expected_out)

    def print_history(self):
        print("\n".join(self.full_output_history))

    def close(self):
        if self.process:
            self.process.terminate()
            self.process.wait()

    def fail_test(self, error_msg: str):
        sys.stderr.write(f"FAIL: {error_msg}\n")
        sys.stderr.flush()
        self.close()
        raise SystemExit(1)