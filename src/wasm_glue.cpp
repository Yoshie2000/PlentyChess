#ifdef ARCH_WASM

#include <string>
#include <deque>
#include <cstring>
#include <sstream>

// Forward declaration of initNetworkData from nnue.cpp
extern void initNetworkData();

// Forward declaration of processCommand from uci.cpp
extern void processCommand(const std::string& line);

// Command queue for receiving commands from JavaScript
static std::deque<std::string> commandQueue;

// Output buffer for sending output to JavaScript
static std::stringstream outputBuffer;

// Flag to check if engine is initialized
static bool wasmInitialized = false;

extern "C" {

// Initialize the engine (called from JavaScript)
void wasm_init() {
    initNetworkData();
    wasmInitialized = true;
}

// Process all pending commands (called from JavaScript after sending commands)
void wasm_process_commands() {
    std::string cmd;
    while (!commandQueue.empty()) {
        cmd = commandQueue.front();
        commandQueue.pop_front();
        processCommand(cmd);
    }
}

// Send a command to the engine (called from JavaScript)
void wasm_send_command(const char* cmd) {
    if (cmd) {
        commandQueue.push_back(std::string(cmd));
    }
}

// Get output from the engine (called from JavaScript)
// Returns a pointer to a static buffer that is valid until the next call
const char* wasm_get_output() {
    static std::string outputStr;
    outputStr = outputBuffer.str();
    outputBuffer.str("");
    outputBuffer.clear();
    return outputStr.c_str();
}

// Check if there are pending commands
bool wasm_has_command() {
    return !commandQueue.empty();
}

// Get the next command (for internal use)
bool wasm_get_next_command(std::string& out) {
    if (commandQueue.empty()) {
        return false;
    }
    out = commandQueue.front();
    commandQueue.pop_front();
    return true;
}

// Redirect output for Wasm (called from engine code)
void wasm_output(const std::string& str) {
    outputBuffer << str;
}

} // extern "C"

// Override std::cout for Wasm
#include <iostream>

namespace WasmIO {
    class WasmStreamBuf : public std::streambuf {
    protected:
        virtual int overflow(int c) override {
            if (c != EOF) {
                char ch = static_cast<char>(c);
                outputBuffer.put(ch);
            }
            return c;
        }

        virtual std::streamsize xsputn(const char* s, std::streamsize n) override {
            outputBuffer.write(s, n);
            return n;
        }
    };

    static WasmStreamBuf wasmStreamBuf;

    void initWasmIO() {
        std::cout.rdbuf(&wasmStreamBuf);
        std::cerr.rdbuf(&wasmStreamBuf);
    }
}

#endif // ARCH_WASM
