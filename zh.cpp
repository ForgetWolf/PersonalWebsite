#include <iostream>
#include <fcntl.h>
#include <io.h>
#include "chinese.hpp"

int main() {
    _setmode(_fileno(stdout), _O_BINARY);
    std::cout << "Content-Type: text/html; charset=utf-8\r\n"
                 "Cache-Control: no-store\r\n\r\n"
              << u8R"HTML(<!doctype html>
<html lang="zh-CN"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>你好，我是ForgetWolf</title>
</head><body>
)HTML" << chinese_svg << "\n</body></html>\n";
}
