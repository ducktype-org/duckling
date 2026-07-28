# Debugger CLI Advanced Integration Tests (itests)

## Test File Format

### First Line

Contains space-separated arguments to be appended to the `VM run -d` command.

### Rest of the File

Contains the actual test script. 
A valid line can be empty, a comment (must start with `#`), or an executable command.

Currently, the test runner supports 3 commands. Each command takes a single argument, separated by a space:

- **`>` (Expect):** Asserts that its argument appears in the VM output.
- **`<` (Write):** Sends its argument directly to the VM's standard input.
- **`timeout`:** Sets the timeout duration (in seconds) for all subsequent expect commands. The default timeout is 1 second.

Because Python expressions support standard comments, inline comments following any of the commands above are also valid.

#### Expectation Matching

Expect commands do not look for an exact full-text match. Instead, they scan the VM output sequentially for the first occurrence of the target argument following the previous successful match. This makes the tests resilient to minor UI changes, as long as the critical output elements remain in order.

### Example

See [simple.test](simple.test) for a practical implementation.