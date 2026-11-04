#include <fcntl.h>
#include <io.h>
#include <iostream>

int main() {
    _setmode(_fileno(stdout), _O_BINARY);
    std::cout << "Content-Type: text/html; charset=utf-8\r\n"
                 "Cache-Control: no-store\r\n\r\n"
              << R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="color-scheme" content="light dark">
<title>Hello World</title>
</head>
<body>hello world</body>
</html>
)HTML";
}
