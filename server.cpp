#include <winsock2.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <cctype>
namespace fs = std::filesystem;

// Read-only static preview host. No CGI, shell commands or page generation.
fs::path resolve(const fs::path& root, std::string target) {
    target = target.substr(0, target.find('?'));
    if (target.empty() || target[0] != '/') return {};
    for (unsigned char c : target)
        if (!(std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.')) return {};
    fs::path relative = target.substr(1);
    for (const auto& part : relative)
        if (part == ".." || part == ".") return {};
    auto path = root / relative;
    std::error_code error;
    if (fs::is_directory(path, error)) path /= "index.html";
    if (!fs::is_regular_file(path, error)) return {};
    // Reject symlinks escaping the published directory as well as traversal.
    auto canonical = fs::weakly_canonical(path, error);
    if (error) return {};
    auto contained = canonical.lexically_relative(root);
    if (contained.empty() || *contained.begin() == "..") return {};
    return canonical;
}
int main(int argc, char** argv) {
    const auto root = fs::weakly_canonical("dist");
    if (argc == 3 && std::string(argv[1]) == "--resolve") {
        const auto path = resolve(root, argv[2]);
        if (path.empty()) return 1;
        std::cout << path.string() << '\n'; return 0;
    }
    if (!fs::exists(root / "index.html")) { std::cerr << "Build dist/ first\n"; return 1; }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2,2), &data)) return 1;
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(8081);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(listener, SOMAXCONN)) {
        std::cerr << "Cannot listen on 0.0.0.0:8081\n";
        closesocket(listener); WSACleanup(); return 1;
    }
    std::cout << "Static preview: http://127.0.0.1:8081/\n";
    for (;;) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        DWORD timeout = 3000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        std::string request;
        char buffer[4096];
        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
            int count = recv(client, buffer, sizeof(buffer), 0);
            if (count <= 0) break;
            request.append(buffer, count);
        }
        auto first = request.find(' '), last = request.find(' ', first == std::string::npos ? 0 : first+1);
        std::string method = request.substr(0, first);
        std::string body, status = "404 Not Found", type = "text/plain; charset=utf-8";
        if ((method == "GET" || method == "HEAD") && first != std::string::npos && last != std::string::npos) {
            auto path = resolve(root, request.substr(first+1, last-first-1));
            if (!path.empty()) {
                std::ifstream file(path, std::ios::binary);
                if (file) {
                    body.assign(std::istreambuf_iterator<char>(file), {});
                    status = "200 OK";
                    if (path.extension() == ".html") type = "text/html; charset=utf-8";
                    else if (path.extension() == ".css") type = "text/css";
                    else if (path.extension() == ".woff") type = "font/woff";
                    else if (path.extension() == ".svg") type = "image/svg+xml";
                    else type = "application/octet-stream";
                }
            }
        }
        std::string response = "HTTP/1.1 " + status + "\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        if (method != "HEAD") response += body;
        std::size_t sent = 0;
        while (sent < response.size()) {
            int count = send(client, response.data()+sent, static_cast<int>(response.size()-sent), 0);
            if (count <= 0) break;
            sent += count;
        }
        closesocket(client);
    }
    closesocket(listener); WSACleanup();
}
