#include <iostream>
#include <fcntl.h>
#include <io.h>
#include "greeting.hpp"

int main() {
    _setmode(_fileno(stdout), _O_BINARY);
    std::cout << "Content-Type: text/html; charset=utf-8\r\n"
                 "Cache-Control: no-store\r\n\r\n"
              << R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>Hello, ForgetWolf here.</title>
</head>
<body>
)HTML"
              << greeting_svg
              << "\n</body>\n</html>\n";
    return 0;
}
