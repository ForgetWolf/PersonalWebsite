#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <iostream>
#include <iterator>

int main() {
    _setmode(_fileno(stdout), _O_BINARY);
    std::ifstream file("carousel/carousel.svg", std::ios::binary);
    if (!file) {
        std::cerr << "Cannot read carousel/carousel.svg\n";
        return 1;
    }
    const std::string svg((std::istreambuf_iterator<char>(file)), {});
    std::cout << "Content-Type: text/html; charset=utf-8\r\n"
                 "Cache-Control: no-store\r\n\r\n"
              << R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>ForgetWolf - Greetings</title>
</head>
<body>
)HTML" << svg << "\n</body>\n</html>\n";
}
