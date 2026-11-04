#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <iostream>
#include <iterator>
#include "random/order_policy.hpp"

int main(int argc, char** argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    std::random_device entropy;
    std::mt19937 rng(entropy());
    const auto order = randomOrder(rng);
    // Future IP-based policy is intentionally commented out in order_policy.hpp.
    const bool mobile = argc == 2 && std::string(argv[1]) == "--mobile";
    const std::string path = std::string(mobile ? "mobile/" : "random/") + order[0] + "-" + order[1] + "-" + order[2] + ".svg";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot read random greeting SVG\n";
        return 1;
    }
    const std::string svg((std::istreambuf_iterator<char>(file)), {});
    std::cout << "Content-Type: text/html; charset=utf-8\r\n"
                 "Cache-Control: no-store\r\n\r\n"
              << R"HTML(<!doctype html>
<html lang="en" id="randomPage">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>ForgetWolf</title>
</head>
<body>
)HTML" << svg << "\n</body>\n</html>\n";
}
