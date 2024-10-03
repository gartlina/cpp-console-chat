#include "server.h"
#include <iostream>    // ��� ������ � �������
#include <thread>      // ��� ������ � ��������
#include <sstream>     // ��� ������ �� ���������� ��������

#pragma comment(lib, "Ws2_32.lib")  // ���������� ���������� �������

// ����������� �������
Server::Server(int port)
    : port_(port), listenSocket_(INVALID_SOCKET) {
    // �������������� Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  // ����������� ������ 2.2
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;  // ������� ������
        exit(1);  // ������� �� ���������
    }

    // ������� ��������� �����
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // ������� TCP-�����
    if (listenSocket_ == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;  // ������� ������
        WSACleanup();  // ������� ������� Winsock
        exit(1);  // ������� �� ���������
    }

    // ����������� ����� ��� �������������
    sockaddr_in service;
    service.sin_family = AF_INET;  // ���������� IPv4
    service.sin_addr.s_addr = INADDR_ANY;  // ��������� ���������� � ����� �������
    service.sin_port = htons(port_);  // ������������� ����

    // ��������� ����� � �������
    if (bind(listenSocket_, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << std::endl;  // ������� ������
        closesocket(listenSocket_);  // ��������� �����
        WSACleanup();  // ������� ������� Winsock
        exit(1);  // ������� �� ���������
    }

    // �������� ������������� �������� ����������
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << std::endl;  // ������� ������
        closesocket(listenSocket_);  // ��������� �����
        WSACleanup();  // ������� ������� Winsock
        exit(1);  // ������� �� ���������
    }

    std::cout << "Server started on port " << port_ << std::endl;  // �������� � ������� �������
}

// ���������� �������
Server::~Server() {
    // ��������� ��� ���������� ������
    for (auto& pair : clients_) {
        closesocket(pair.first);  // ��������� ����� �������
    }
    // ��������� ��������� �����
    closesocket(listenSocket_);
    // ������� ������� Winsock
    WSACleanup();
}

// ������ �������
void Server::run() {
    while (true) {
        sockaddr_in clientInfo;  // ���������� � �������
        int clientInfoSize = sizeof(clientInfo);
        // ��������� �������� ����������
        SOCKET clientSocket = accept(listenSocket_, (SOCKADDR*)&clientInfo, &clientInfoSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;  // ������� ������
            continue;  // ��������� � ��������� ��������
        }

        std::cout << "New connection accepted: " << clientSocket << std::endl;  // �������� � ����� ����������

        // ������������ ������� � ��������� ������
        std::thread(&Server::handleClient, this, clientSocket).detach();  // ��������� ����� � ����������� ���
    }
}

// ��������� �������
void Server::handleClient(SOCKET clientSocket) {
    char recvbuf[512];  // ����� ��� ������ ������
    int recvbuflen = 512;  // ������ ������
    std::string leftover;  // ��� �������� �������� ������

    User currentUser;  // ������� ������������
    bool isAuthenticated = false;  // ���� ��������������

    while (true) {
        int iResult = recv(clientSocket, recvbuf, recvbuflen, 0);  // �������� �������� ������ �� �������
        if (iResult > 0) {
            leftover += std::string(recvbuf, iResult);  // ��������� ���������� ������

            size_t pos;
            while ((pos = leftover.find('\n')) != std::string::npos) {  // ���� ������ ������
                std::string line = leftover.substr(0, pos);  // ��������� ������ �������
                leftover.erase(0, pos + 1);  // ������� ������������ ������ �� ������

                std::istringstream iss(line);
                std::string command;
                iss >> command;

                if (command == "REGISTER") {
                    // ����������� ��� ���� ������: ������ �����
                    std::string login, password, name;
                    iss >> login >> password >> name;
                    currentUser.id = ++userIdCounter_;
                    currentUser.login = login;
                    currentUser.name = name;
                    isAuthenticated = true;
                    clients_[clientSocket] = currentUser;
                    userSockets_[currentUser.id] = clientSocket;

                    std::string response = "REGISTER_SUCCESS\n";
                    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
                    std::cout << "User registered: " << login << std::endl;
                }
                else if (command == "LOGIN") {
                    // ���� ��� ���� ������: ���������� ������������
                    std::string login, password;
                    iss >> login >> password;
                    currentUser.id = ++userIdCounter_;
                    currentUser.login = login;
                    currentUser.name = login;  // ���������� ����� ��� ���
                    isAuthenticated = true;
                    clients_[clientSocket] = currentUser;
                    userSockets_[currentUser.id] = clientSocket;

                    std::ostringstream oss;
                    oss << "LOGIN_SUCCESS " << currentUser.id << " " << currentUser.name << "\n";
                    std::string response = oss.str();
                    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
                    std::cout << "User logged in: " << login << std::endl;
                }
                else if (command == "MESSAGE") {
                    if (!isAuthenticated) {
                        std::string response = "NOT_AUTHENTICATED\n";
                        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
                        continue;
                    }
                    int senderId;
                    int receiverId;
                    iss >> senderId >> receiverId;
                    std::string content;
                    std::getline(iss, content);
                    if (!content.empty() && content[0] == ' ') {
                        content.erase(0, 1);
                    }

                    // �������� ����� ����������� � ����������
                    std::string senderName = currentUser.name;
                    std::string receiverName;

                    if (receiverId == 0) {
                        receiverName = "All";
                    }
                    else {
                        // ���� ���������� ����� ������������ �������������
                        auto it = clients_.find(userSockets_[receiverId]);
                        if (it != clients_.end()) {
                            receiverName = it->second.name;
                        }
                        else {
                            receiverName = "Unknown";
                        }
                    }

                    // ��������� ��������� � �������
                    std::ostringstream formattedMsg;
                    formattedMsg << "NEW_MESSAGE " << senderId << " " << receiverId << " "
                        << senderName << " " << receiverName << " " << content << "\n";

                    if (receiverId == 0) {
                        // ����������������� ���������
                        broadcastMessage(formattedMsg.str());  // ��������� ����
                        std::cout << "Broadcast message from " << senderName << ": " << content << std::endl;
                    }
                    else {
                        // ������ ���������
                        auto it = userSockets_.find(receiverId);
                        if (it != userSockets_.end()) {
                            // ������������ ������
                            send(it->second, formattedMsg.str().c_str(), static_cast<int>(formattedMsg.str().size()), 0);  // ���������� ���������
                            std::cout << "Private message from " << senderName << " to " << receiverName << ": " << content << std::endl;
                        }
                        else {
                            // ������������ ������
                            std::cout << "User " << receiverId << " is not connected. Message not delivered." << std::endl;
                        }
                    }
                }
                else if (command == "LOGOUT") {
                    if (!isAuthenticated) {
                        std::string response = "NOT_AUTHENTICATED\n";
                        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
                        continue;
                    }
                    int senderId;
                    iss >> senderId;

                    // ������� ������������ �� �������
                    clients_.erase(clientSocket);
                    userSockets_.erase(senderId);

                    std::cout << "User logged out: " << senderId << std::endl;

                    // ���������� �������������
                    std::string response = "LOGOUT_SUCCESS\n";
                    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);

                    // ��������� ����������
                    break;
                }
                else {
                    std::string response = "UNKNOWN_COMMAND\n";
                    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
                    std::cout << "Unknown command received: " << command << std::endl;
                }
            }
        }
        else if (iResult == 0) {
            std::cout << "Connection closed by client." << std::endl;
            break;  // ������ ������ ����������
        }
        else {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            break;  // ������ ��������� ������
        }
    }

    // ������� ��������
    closesocket(clientSocket);  // ��������� ����� �������
    clients_.erase(clientSocket);  // ������� ������� �� ������
    for (auto it = userSockets_.begin(); it != userSockets_.end();) {
        if (it->second == clientSocket) {
            it = userSockets_.erase(it);  // ������� ������
        }
        else {
            ++it;
        }
    }
}

// �������� ��������� ���� ������������ ��������
void Server::broadcastMessage(const std::string& message) {
    for (const auto& pair : clients_) {
        send(pair.first, message.c_str(), static_cast<int>(message.size()), 0);  // ���������� ��������� ������� �������
    }
}