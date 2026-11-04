#include <iostream>
#include <fcntl.h>
#include <io.h>
#include "russian.hpp"

int main() {
    _setmode(_fileno(stdout), _O_BINARY);
    std::cout << "Content-Type: text/html; charset=utf-8\r\n"
                 "Cache-Control: no-store\r\n\r\n"
              << "<!doctype html>\n<html lang=\"ru\">\n<head>\n"
                 "<meta charset=\"utf-8\">\n"
                 "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                 "<meta name=\"color-scheme\" content=\"light dark\">\n<title>"
              << russian_label << "</title>\n</head>\n<body>\n"
              << russian_svg << "\n</body>\n</html>\n";
}
