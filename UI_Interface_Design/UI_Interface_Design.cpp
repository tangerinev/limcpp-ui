#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <cctype>

// Link with the Winsock library
#pragma comment(lib, "Ws2_32.lib")

// Function to decode URL-encoded characters
std::string url_decode(const std::string& input) {
    std::string decoded;
    char ch;
    int ii;
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '%') {
            if (i + 2 < input.length()) {
                if (sscanf_s(input.substr(i + 1, 2).c_str(), "%x", &ii) == 1) {
                    ch = static_cast<char>(ii);
                    decoded += ch;
                    i += 2;
                }
            }
        }
        else if (input[i] == '+') {
            decoded += ' ';
        }
        else {
            decoded += input[i];
        }
    }
    return decoded;
}

std::string get_current_time() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm;
    localtime_s(&local_tm, &now);

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(buffer);
}

std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        return "";
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

std::string create_echo_response(const std::string& input) {
    // Format the response as plain text with the message and timestamp
    std::string response = "You said: \"" + input + "\" at " + get_current_time();
    std::ostringstream http_response;
    http_response << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/plain\r\n"
        << "Content-Length: " << response.size() << "\r\n"
        << "\r\n"
        << response;

    return http_response.str();
}

std::string create_file_response(const std::string& file_content) {
    std::ostringstream http_response;
    http_response << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/html\r\n"
        << "Content-Length: " << file_content.size() << "\r\n"
        << "\r\n"
        << file_content;

    return http_response.str();
}

std::string parse_input_from_request(const std::string& request) {
    std::string search_string = "GET /echo?input=";
    size_t start_pos = request.find(search_string);
    if (start_pos == std::string::npos) {
        return "";
    }

    start_pos += search_string.length();
    size_t end_pos = request.find(" ", start_pos);
    std::string encoded_input = request.substr(start_pos, end_pos - start_pos);

    // Decode the URL-encoded input
    return url_decode(encoded_input);
}

bool is_root_request(const std::string& request) {
    return request.find("GET / ") != std::string::npos;
}

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    SOCKET server_socket;
    struct sockaddr_in server_address;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind socket to port 8080
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is running on http://localhost:8080" << std::endl;

    while (true) {
        SOCKET client_socket;
        struct sockaddr_in client_address;
        int client_address_len = sizeof(client_address);

        // Accept a new connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return 1;
        }

        // Use dynamic memory allocation to reduce stack usage
        auto buffer = std::make_unique<char[]>(30000);
        int bytes_received = recv(client_socket, buffer.get(), 30000, 0);

        if (bytes_received > 0) {
            std::string request(buffer.get());
            std::string http_response;

            if (is_root_request(request)) {
                std::string html_content = read_file("index.html");
                if (!html_content.empty()) {
                    http_response = create_file_response(html_content);
                }
                else {
                    http_response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
                }
            }
            else {
                std::string user_input = parse_input_from_request(request);
                if (!user_input.empty()) {
                    http_response = create_echo_response(user_input);
                }
                else {
                    http_response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing input parameter";
                }
            }

            send(client_socket, http_response.c_str(), static_cast<int>(http_response.length()), 0);
        }

        closesocket(client_socket);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
