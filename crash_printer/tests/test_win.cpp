
#include "crash_printer/win.hpp"
#include "common_helpers/common_helpers.hpp"

#include <iostream>
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

std::string logs_filepath = "./crash_test/zxc.txt";


static LONG WINAPI exception_handler(LPEXCEPTION_POINTERS ex_pointers)
{
    std::ifstream file(std::filesystem::u8path(logs_filepath));
    if (file.is_open()) {
        std::string line{};
        std::getline(file, line);
        file.close();

        if (line.size()) {
            std::cout << "Success!" << std::endl;
            exit(0);
        }
    }
    
    std::cerr << "Failed!" << std::endl;
    exit(1);
}

int* get_ptr()
{
    return nullptr;
}

int main()
{
    // simulate the existence of previous handler
    SetUnhandledExceptionFilter(exception_handler);

    if (!common_helpers::remove_file(logs_filepath)) {
        std::cerr << "failed to remove log" << std::endl;
        return 1;
    }

    if (!crash_printer::init(logs_filepath)) {
        std::cerr << "failed to init" << std::endl;
        return 1;
    }

    // simulate a crash
    volatile int * volatile ptr = get_ptr();
    *ptr = 42;

    return 0;
}
