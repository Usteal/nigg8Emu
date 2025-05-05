#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <windows.h>

class SimpleIO { // todo: make it grid based
    HWND hwnd = nullptr;
    HINSTANCE hInstance = nullptr;
    bool graphicsMode;
    std::string latestInput;
    POINT mousePos = {0, 0};
    bool mouseClicked = false;
    int width = 800, height = 600;
    bool fullscreen = false;
    WINDOWPLACEMENT prevPlacement = {};
    HDC backBufferDC = nullptr;
    HBITMAP backBufferBitmap = nullptr;

    static SimpleIO* instance;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_DESTROY) PostQuitMessage(0);

        if (instance && instance->graphicsMode) {
            switch (uMsg) {
            case WM_SIZE:
                instance->resize(LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_KEYDOWN:
                if (wParam == VK_F11) instance->toggle_fullscreen();
                else instance->latestInput = static_cast<char>(wParam);
                break;
            case WM_LBUTTONDOWN:
                instance->mouseClicked = true;
                instance->mousePos.x = LOWORD(lParam);
                instance->mousePos.y = HIWORD(lParam);
                break;
            case WM_MOUSEMOVE:
                instance->mousePos.x = LOWORD(lParam);
                instance->mousePos.y = HIWORD(lParam);
                break;
            case WM_PAINT:
                instance->paint();
                ValidateRect(hwnd, nullptr);
                break;
            }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void set_mode(bool useGraphics) {
        this->graphicsMode = useGraphics;
        clear_screen(); // optional: clear when switching
    }

    void create_backbuffer() {
        HDC hdc = GetDC(hwnd);
        if (backBufferDC) DeleteDC(backBufferDC);
        if (backBufferBitmap) DeleteObject(backBufferBitmap);

        backBufferDC = CreateCompatibleDC(hdc);
        backBufferBitmap = CreateCompatibleBitmap(hdc, width, height);
        SelectObject(backBufferDC, backBufferBitmap);
        ReleaseDC(hwnd, hdc);

        clear_screen(); // clear newly created backbuffer
    }

    void paint() {
        HDC hdc = GetDC(hwnd);
        BitBlt(hdc, 0, 0, width, height, backBufferDC, 0, 0, SRCCOPY);
        ReleaseDC(hwnd, hdc);
    }

    void resize(int w, int h) {
        width = w;
        height = h;
        create_backbuffer();
    }

    void toggle_fullscreen() {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        if (!fullscreen) {
            MONITORINFO mi = { sizeof(mi) };
            if (GetWindowPlacement(hwnd, &prevPlacement) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                fullscreen = true;
            }
        } else {
            SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(hwnd, &prevPlacement);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen = false;
        }
    }

public:
    SimpleIO(bool useGraphics) : graphicsMode(useGraphics) {
        instance = this;
        if (graphicsMode) {
            hInstance = GetModuleHandle(nullptr);
            const wchar_t CLASS_NAME[] = L"SimpleWindowClass";

            WNDCLASS wc = {};
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = hInstance;
            RegisterClass(&wc);

            hwnd = CreateWindowExW(
                0, CLASS_NAME, L"SimpleIO Window",
                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                nullptr, nullptr, hInstance, nullptr
            );
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
            create_backbuffer();
        }
    }

    ~SimpleIO() {
        if (graphicsMode) {
            if (backBufferDC) DeleteDC(backBufferDC);
            if (backBufferBitmap) DeleteObject(backBufferBitmap);
            if (hwnd) DestroyWindow(hwnd);
        }
    }

    void print(const std::string& text) {
        if (graphicsMode) {
            TextOutA(backBufferDC, 10, 10, text.c_str(), (int)text.size());
            paint();
        } else {
            std::cout << text;
        }
    }

    std::string input() {
        if (graphicsMode) {
            latestInput.clear();
            while (latestInput.empty()) {
                MSG msg;
                if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
            std::string res = latestInput;
            latestInput.clear();
            return res;
        } else {
            std::string s;
            std::getline(std::cin, s);
            return s;
        }
    }

    void draw_rect(int x, int y, int w, int h, COLORREF color) {
        if (graphicsMode) {
            HBRUSH brush = CreateSolidBrush(color);
            RECT rect = { x, y, x + w, y + h };
            FillRect(backBufferDC, &rect, brush);
            DeleteObject(brush);
            paint();
        }
    }

    void draw_circle(int x, int y, int r, COLORREF color) {
        if (graphicsMode) {
            HBRUSH brush = CreateSolidBrush(color);
            HBRUSH oldBrush = (HBRUSH)SelectObject(backBufferDC, brush);
            Ellipse(backBufferDC, x - r, y - r, x + r, y + r);
            SelectObject(backBufferDC, oldBrush);
            DeleteObject(brush);
            paint();
        }
    }

    void color_text(const std::string& text, COLORREF color, int x, int y) {
        if (graphicsMode) {
            SetTextColor(backBufferDC, color);
            SetBkMode(backBufferDC, TRANSPARENT);
            TextOutA(backBufferDC, x, y, text.c_str(), (int)text.size());
            paint();
        }
    }

    void clear_screen(COLORREF color = RGB(255, 255, 255)) {
        if (graphicsMode) {
            HBRUSH brush = CreateSolidBrush(color);
            RECT rect = { 0, 0, width, height };
            FillRect(backBufferDC, &rect, brush);
            DeleteObject(brush);
            paint();
        } else {
            system("cls");
        }
    }

    void draw_line(int x1, int y1, int x2, int y2, COLORREF color) {
        if (graphicsMode) {
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HPEN oldPen = (HPEN)SelectObject(backBufferDC, pen);

            MoveToEx(backBufferDC, x1, y1, nullptr);
            LineTo(backBufferDC, x2, y2);

            SelectObject(backBufferDC, oldPen);
            DeleteObject(pen);
            paint();
        }
    }

    bool mouse_clicked() {
        if (graphicsMode) {
            bool clicked = mouseClicked;
            mouseClicked = false;
            return clicked;
        }
        return false;
    }

    POINT get_mouse_pos() {
        return mousePos;
    }

    void message_loop() {
        if (graphicsMode) {
            MSG msg;
            while (GetMessage(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
};

SimpleIO* SimpleIO::instance = nullptr;
SimpleIO io(false);

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
                uint8_t value = fetch_operand(mode, data, port);
                std::cout << static_cast<char>(value); // TODO: use SimpleIO
                const uint8_t PORT_PRINT        = 0x00;
                const uint8_t PORT_DRAW_RECT    = 0x01;
                const uint8_t PORT_DRAW_CIRCLE  = 0x02;
                const uint8_t PORT_DRAW_LINE    = 0x03;
                
                switch (port) {
                    case PORT_PRINT:
                    {
                        std::string s(1, (char)data);
                        io.print(s);
                        break;
                    }
                    case PORT_DRAW_RECT:
                    {
                        io.draw_rect(50, 50, data, data, RGB(0, 0, 255));
                        break;
                    }
                    case PORT_DRAW_CIRCLE:
                    {
                        io.draw_circle(100, 100, data, RGB(255, 0, 0));
                        break;
                    }
                    case PORT_DRAW_LINE:
                    {
                        io.draw_line(10, 10, 10 + data, 10 + data, RGB(0, 255, 0));
                        break;
                    }
                }
                

                break;
            }

            case 0x02: { // in
                uint8_t mode = memory[pc++];
                uint8_t dest = memory[pc++];
                uint8_t port = memory[pc++];
                uint8_t value;
                std::cin >> value; // TODO: use SimpleIO
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

            case 0x0f: { // (unassigned)(uninmplemented) int
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
