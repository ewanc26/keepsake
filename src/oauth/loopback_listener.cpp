#include "oauth/loopback_listener.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace keepsake::oauth {

namespace {

constexpr char kResponseBody[] =
    "<!doctype html><html><head><title>Keepsake</title></head>"
    "<body style=\"font-family: sans-serif; max-width: 32em; margin: 4em "
    "auto;\">"
    "<p>Signed in. You can close this tab and return to the terminal.</p>"
    "</body></html>";

// Extracts the query string from an HTTP request line of the form
// "GET /callback?a=b&c=d HTTP/1.1". Returns "" if there's no '?'.
std::string parseQueryFromRequestLine(const std::string &line) {
    size_t pathStart = line.find(' ');
    if (pathStart == std::string::npos) return "";
    pathStart += 1;
    size_t pathEnd = line.find(' ', pathStart);
    if (pathEnd == std::string::npos) return "";
    std::string target = line.substr(pathStart, pathEnd - pathStart);
    size_t q = target.find('?');
    if (q == std::string::npos) return "";
    return target.substr(q + 1);
}

} // namespace

CallbackResult waitForCallback(int port, int timeoutSeconds) {
    CallbackResult result;

    int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        result.error = "socket() failed";
        return result;
    }

    int reuse = 1;
    ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
        result.error = "bind() failed — port " + std::to_string(port) +
                       " may already be in use";
        ::close(listenFd);
        return result;
    }

    if (::listen(listenFd, 1) < 0) {
        result.error = "listen() failed";
        ::close(listenFd);
        return result;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(listenFd, &readSet);
    timeval timeout{timeoutSeconds, 0};

    int selectResult =
        ::select(listenFd + 1, &readSet, nullptr, nullptr, &timeout);
    if (selectResult <= 0) {
        result.error = "timeout waiting for the browser to redirect back";
        ::close(listenFd);
        return result;
    }

    int clientFd = ::accept(listenFd, nullptr, nullptr);
    ::close(listenFd);
    if (clientFd < 0) {
        result.error = "accept() failed";
        return result;
    }

    std::string request;
    char buf[2048];
    while (request.find("\r\n") == std::string::npos &&
           request.size() < sizeof(buf) * 4) {
        ssize_t n = ::recv(clientFd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        request.append(buf, static_cast<size_t>(n));
    }

    size_t lineEnd = request.find("\r\n");
    std::string requestLine =
        lineEnd == std::string::npos ? request : request.substr(0, lineEnd);

    std::string response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Content-Length: " +
                           std::to_string(sizeof(kResponseBody) - 1) +
                           "\r\n"
                           "Connection: close\r\n\r\n" +
                           kResponseBody;
    ::send(clientFd, response.data(), response.size(), 0);
    ::close(clientFd);

    std::string query = parseQueryFromRequestLine(requestLine);
    if (query.empty()) {
        result.error = "redirect had no query string";
        return result;
    }

    result.ok = true;
    result.rawQuery = query;
    return result;
}

} // namespace keepsake::oauth
