#include <tester/tester.hpp>
#include <vm/debugger/UI/debug_adapter/debug_adapter.hpp>
#include <sstream>
#include <iostream>

class DebugAdapterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DebugAdapterTest

public:
    TESTER_TEST_SIMPLE_CONSTRUCTOR() {
        TESTER_ADD_TEST(testFullInitializeFlow);
    }

private:
    std::string formatDAPMessage(const std::string& json_body) {
        return "Content-Length: " + std::to_string(json_body.size()) + "\r\n\r\n" + json_body;
    }

    void testFullInitializeFlow() {
        // 1. Create string streams for our fake VS Code input and adapter output
        std::stringstream mock_vs_code_input;
        std::stringstream mock_adapter_output;

        // 2. Load the fake input with the exact bytes VS Code would send
        std::string init_req = R"({"seq": 1, "type": "request", "command": "initialize", "arguments": {}})";
        mock_vs_code_input << formatDAPMessage(init_req);

        // Optional: You can queue multiple messages at once!
        std::string threads_req = R"({"seq": 2, "type": "request", "command": "threads"})";
        mock_vs_code_input << formatDAPMessage(threads_req);

        // 3. HIJACK std::cin AND std::cout
        std::streambuf* original_cin = std::cin.rdbuf(mock_vs_code_input.rdbuf());
        std::streambuf* original_cout = std::cout.rdbuf(mock_adapter_output.rdbuf());
        // 4. Run the adapter. 
        // Because mock_vs_code_input only has a finite amount of text, std::getline(std::cin)
        // will eventually hit EOF (End of File) and naturally break out of the while loop!
        auto adapter = DebugAdapter(fs::File(path("debugger_test.dbc")));
        adapter.run();

        // 5. RESTORE std::cin AND std::cout (CRITICAL: Do this before any ASSERTs!)
        std::cin.rdbuf(original_cin);
        std::cout.rdbuf(original_cout);
        std::cin.clear();
        // 6. Check the captured output
        std::string response = mock_adapter_output.str();

        // Validate the Initialize Response
        ASSERT_TRUE(response.find("\"command\":\"initialize\"") != std::string::npos);
        ASSERT_TRUE(response.find("\"success\":true") != std::string::npos);

        // Validate the Event was sent
        ASSERT_TRUE(response.find("\"event\":\"initialized\"") != std::string::npos);

        // Validate the Threads Response (since we queued it up too)
        ASSERT_TRUE(response.find("\"command\":\"threads\"") != std::string::npos);
        ASSERT_TRUE(response.find("Main Thread") != std::string::npos);

    }
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");