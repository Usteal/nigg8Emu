#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <fstream>

std::vector<uint8_t> get_program(const std::string& filepath) {
    std::vector<uint8_t> program;
    std::ifstream file(filepath, std::ios::binary); // Open file in binary mode

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    };

    uint8_t byte;
    while (file.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        program.push_back(byte); // Read one byte at a time and push to vector
    };

    file.close();

    return program;
};

class VirtualMachine {
private:
    static const size_t MEMORY_SIZE = 256;
    static const size_t NUM_REGISTERS = 256; // Covers all register IDs (0x01–0x24)
    static const double CLOCK_PERIOD_MS; // 1000 ms / 16 Hz = 62.5 ms

    std::vector<uint8_t> memory; // 256 bytes of RAM
    std::vector<uint8_t> registers; // Register file (r1–r4, e1–e4, x1–x4/l1–l4)
    uint8_t pc; // Program counter (8-bit address)
    uint8_t sp; // Stack pointer
    bool running; // VM state
    bool flag_equal, flag_less, flag_more; // Comparison flags

public:
    VirtualMachine() : memory(MEMORY_SIZE, 0), registers(NUM_REGISTERS, 0),
                       pc(0), sp(0xff), running(false),
                       flag_equal(false), flag_less(false), flag_more(false) {}

    // Load program into memory
    void load_program(const std::vector<uint8_t>& program) {
        if (program.size() > MEMORY_SIZE) {
            throw std::runtime_error("Program too large for memory");
        }
        for (size_t i = 0; i < program.size(); ++i) {
            memory[i] = program[i];
        }
        
        // Debug: Print out loaded program
        for (size_t i = 0; i < program.size(); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)program[i] << " ";
        }        
        std::cout << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
    }

    // Run the VM at 16 Hz
    void run() {
        running = true;
        while (running) {
            auto start = std::chrono::steady_clock::now();
            execute_instruction();
            auto end = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            auto sleep_ms = static_cast<long>(CLOCK_PERIOD_MS - elapsed_ms);
            if (sleep_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }
        }
    }

private:
    // Fetch value based on addressing mode
    uint8_t fetch_operand(uint8_t mode, uint8_t operand1, uint8_t operand2) {
        switch (mode) {
            case 0x00: case 0x10: case 0x20: case 0x30: return operand1; // Immediate
            case 0x01: case 0x11: case 0x21: case 0x31: return registers[operand1]; // Register
            case 0x02: case 0x12: case 0x22: case 0x32: return memory[operand1]; // Memory
            case 0x03: case 0x13: case 0x23: case 0x33: return memory[registers[operand1]]; // Register indirect
            default: throw std::runtime_error("Invalid operand mode");
        }
    }

    // Store value based on addressing mode
    void store_operand(uint8_t mode, uint8_t dest, uint8_t value) {
        switch (mode) {
            case 0x00: case 0x01: case 0x02: case 0x03: dest = value; break;
            case 0x10: case 0x11: case 0x12: case 0x13: registers[dest] = value; break; // Register
            case 0x20: case 0x21: case 0x22: case 0x23: memory[dest] = value; break; // Memory
            case 0x30: case 0x31: case 0x32: case 0x33: memory[registers[dest]] = value; break; // Register indirect
            default: throw std::runtime_error("Invalid destination mode");
        }
    }

    // Execute a single instruction
    void execute_instruction() {
        if (pc >= MEMORY_SIZE) {
            running = false;
            return;
        }

        uint8_t opcode = memory[pc++];

        switch (opcode) {
            case 0x00: // nop
                break;

            case 0x01: { // out
                uint8_t mode = memory[pc++];
                uint8_t data = memory[pc++];
                uint8_t port = memory[pc++];
                uint8_t value = fetch_operand(mode, data, port);  // Fetch value based on mode
                std::cout << static_cast<char>(value);  // Output value as a character
                break;
            }

            case 0x02: { // in
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t port = memory[pc++];
                uint8_t value;
                std::cin >> value;
                store_operand(mode, dest, value);
                break;
            }

            case 0x03: { // lea
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t addr = memory[pc++];
                store_operand(mode, dest, addr);
                break;
            }

            case 0x04: { // mov
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t value = fetch_operand(mode, src, 0);
                store_operand(mode, dest, value);
                break;
            }

            case 0x05: { // ret
                if (sp >= 0xff) {
                    throw std::runtime_error("Stack underflow");
                }
                pc = memory[++sp];
                break;
            }

            case 0x06: { // cal
                uint8_t addr = memory[pc++];
                if (--sp < 0) {
                    throw std::runtime_error("Stack overflow");
                }
                memory[sp] = pc;
                pc = addr;
                break;
            }

            case 0x07: // jmp
                pc = memory[pc];
                break;

            case 0x08: // jl
                if (flag_less) pc = memory[pc];
                else pc++;
                break;

            case 0x09: // jnl
                if (!flag_less) pc = memory[pc];
                else pc++;
                break;

            case 0x0a: // jnm
                if (!flag_more) pc = memory[pc];
                else pc++;
                break;

            case 0x0b: // jm
                if (flag_more) pc = memory[pc];
                else pc++;
                break;

            case 0x0c: // jne
                if (!flag_equal) pc = memory[pc];
                else pc++;
                break;

            case 0x0d: // je
                if (flag_equal) pc = memory[pc];
                else pc++;
                break;

            case 0x0e: { // cmp
                uint8_t mode = memory[pc++];
                uint8_t op1 = memory[pc++];
                uint8_t op2 = memory[pc++];
                uint8_t val1 = fetch_operand(mode, op1, 0);
                uint8_t val2 = fetch_operand(mode >> 4, op2, 0);
                flag_equal = (val1 == val2);
                flag_less = (val1 < val2);
                flag_more = (val1 > val2);
                break;
            }

            case 0x0f: {
                break;
            }

            case 0x10: { // add
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = val1 + val2;
                store_operand(mode, dest, result);
                break;
            }
                
            case 0x11: { // sub
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = val1 - val2;
                store_operand(mode, dest, result);
                break;
            }
                
            case 0x12: { // mul
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = val1 * val2;
                store_operand(mode, dest, result);
                break;
            }
            
            case 0x13: { // div
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result;
            
                // Handle division by zero by returning 0
                if (val2 == 0) {
                    result = 0;
                } else {
                    result = val1 / val2;
                }
                
                store_operand(mode, dest, result);
                break;
            }

            case 0x20: { // and
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = val1 & val2;
                store_operand(mode, dest, result);
                break;
            }

            case 0x21: { // or
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = val1 | val2;
                store_operand(mode, dest, result);
                break;
            }
            
            case 0x22: { // xor
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = val1 ^ val2;
                store_operand(mode, dest, result);
                break;
            }

            case 0x24: { // nor
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = ~(val1 | val2);
                store_operand(mode, dest, result);
                break;
            }
            
            case 0x25: { // nand
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t val1 = fetch_operand(mode, dest, 0);
                uint8_t val2 = fetch_operand(mode, src, 0);
                uint8_t result = ~(val1 & val2);
                store_operand(mode, dest, result);
                break;
            }

            case 0x23: { // not
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t value = fetch_operand(mode, dest, 0);
                store_operand(mode, dest, ~value);
                break;
            }

            case 0x26: { // push
                uint8_t mode = memory[pc++];
                uint8_t src = memory[pc++];
                uint8_t value = fetch_operand(mode, src, 0);
                if (--sp < 0) {
                    throw std::runtime_error("Stack overflow");
                }
                memory[sp] = value;
                break;
            }

            case 0x27: { // pop
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                if (sp >= 0xff) {
                    throw std::runtime_error("Stack underflow");
                }
                uint8_t value = memory[++sp];
                store_operand(mode, dest, value);
                break;
            }
            
            case 0x30: {
                break;
            }

            case 0x31: {
                break;
            }

            case 0xff: // hlt
                running = false;
                break;
            
            default:
                throw std::runtime_error("Unknown opcode: " + std::to_string(opcode));

        }
    }
};

// Define clock period (1000 ms / 16 Hz = 62.5 ms)
const double VirtualMachine::CLOCK_PERIOD_MS = 1000.0 / 16.0;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Error: No input binary" << std::endl;
        return 1;
    }

    VirtualMachine vm;

    // Load program:
    std::vector<uint8_t> program = get_program(argv[1]);

    try {
        vm.load_program(program);
        vm.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}