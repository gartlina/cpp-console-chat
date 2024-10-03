#include "client.h"
#include <iostream>      // ��� �����-������
#include <sstream>       // ��� ������ �� ���������� ��������
#include <ws2tcpip.h>    // ��� ������� �������
#include <string>        // ��� ������ �� ��������
#include <thread>        // ��� ������������� �������
#include <chrono>        // ��� ������ �� ��������
#include <atomic>        // ��� ��������� ����������

#pragma comment(lib, "Ws2_32.lib")  // ���������� ���������� �������

// ����������� �������
Client::Client(const std::string& serverIP, int serverPort)
    : serverIP_(serverIP), serverPort_(serverPort),
    isAuthenticated_(false), userId_(0), isRunning_(true) {
    // �������������� Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  // ����������� ������ 2.2
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;  // ������� ������
        exit(1);  // ������� �� ���������
    }

    // ������� �����
    clientSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // ������� TCP-�����
    if (clientSocket_ == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;  // ������� ������
        WSACleanup();  // ������� ������� Winsock
        exit(1);  // ������� �� ���������
    }

    // ������������� ����� � ������������� �����
    u_long mode = 1;  // 1 �������� ������������� �����
    ioctlsocket(clientSocket_, FIONBIO, &mode);  // ������������� ����� ������

    // ������ ����� �������
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;  // ���������� IPv4
    if (InetPtonA(AF_INET, serverIP_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported" << std::endl;  // ������� ������
        closesocket(clientSocket_);  // ��������� �����
        WSACleanup();  // ������� ������� Winsock
        exit(1);  // ������� �� ���������
    }
    serverAddr.sin_port = htons(serverPort_);  // ������������� ���� �������

    // ������������ � �������
    iResult = connect(clientSocket_, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        int err = WSAGetLastError();  // �������� ��� ������
        if (err != WSAEWOULDBLOCK && err != WSAEISCONN) {  // ���� ������ �� ������� � ������������� �������
            std::cerr << "Unable to connect to server: " << err << std::endl;  // ������� ������
            closesocket(clientSocket_);  // ��������� �����
            WSACleanup();  // ������� ������� Winsock
            exit(1);  // ������� �� ���������
        }
    }

    std::cout << "Connected to server.\n";  // �������� � �����������
}

// ���������� �������
Client::~Client() {
    isRunning_ = false;  // ������������� ������ ������������� ������������
    closesocket(clientSocket_);  // ��������� �����
    WSACleanup();  // ������� ������� Winsock
}

// ������� ������ ��� ������������� ���������
void Client::listenForMessages() {
    char recvbuf[512];  // ����� ��� ������ ������
    int recvbuflen = 512;  // ������ ������
    std::string recvBuffer = leftoverData_;  // �������� � ����� ���������� ������

    while (isRunning_) {  // ���� ������ ��������
        int iResult = recv(clientSocket_, recvbuf, recvbuflen, 0);  // �������� �������� ������

        if (iResult > 0) {
            recvBuffer.append(recvbuf, iResult);  // ��������� ���������� ������ � �����

            size_t pos;
            while ((pos = recvBuffer.find('\n')) != std::string::npos) {  // ���� ������ ���������
                std::string message = recvBuffer.substr(0, pos);  // ��������� ���������
                recvBuffer.erase(0, pos + 1);  // ������� ������������ ��������� �� ������

                // ������ ���������
                std::istringstream iss(message);
                std::string command;
                iss >> command;

                if (command == "NEW_MESSAGE") {
                    int senderId, receiverId;
                    std::string senderName, receiverName;
                    std::string content;
                    iss >> senderId >> receiverId >> senderName >> receiverName;  // ������ �������������� � �����
                    std::getline(iss, content);  // �������� ���������� ���������
                    if (!content.empty() && content[0] == ' ') {
                        content.erase(0, 1); // ������� ��������� ������
                    }

                    if (receiverId == 0) {
                        // ��������� ��� ����
                        std::cout << "\nNew message: [ALL] '" << senderName << "': '" << content << "'\n";
                    }
                    else if (receiverId == userId_) {
                        // ������ ��������� ����� ������������
                        std::cout << "\nNew message: [PM] '" << senderName << "' -> 'You': '" << content << "'\n";
                    }
                    else {
                        // ��������� �� ������������� ����� ������������
                        // ����� ����������� ���������� ��� ������������
                    }
                    std::cout << "\n";  // ��������� ������ ������ ��� ����������
                }
                else {
                    std::cout << "\nReceived message: " << message << std::endl;  // ������� ���������� ���������
                }
            }
        }
        else if (iResult == 0) {
            std::cout << "Connection closed by server.\n";  // ������ ������ ����������
            isRunning_ = false;  // ������������� ������
        }
        else {
            int errCode = WSAGetLastError();  // �������� ��� ������
            if (errCode != WSAEWOULDBLOCK) {
                std::cerr << "recv failed: " << errCode << std::endl;  // ������� ������
                isRunning_ = false;  // ������������� ������ ��� ������
            }
            else {
                // ������ ���, ����, ����� �������� ������������ ������
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
}

// ���� ����� �����������
void Client::authenticatedMenu() {
    int choice = 0;
    while (isAuthenticated_) {
        std::cout << "\n1. Send to all\n2. Send private message\n3. Logout\n4. Exit\nSelect an option: ";
        std::cin >> choice;
        std::cin.ignore(); // ������� ����� �����

        switch (choice) {
        case 1:
            sendMessageToAll();  // ���������� ��������� ����
            break;
        case 2:
            sendPrivateMessage();  // ���������� ������ ���������
            break;
        case 3:
            logout();  // ������� �� ��������
            return;  // ������������ ����� ������
        case 4:
            std::cout << "Exiting...\n";  // ������� �� ���������
            isRunning_ = false;  // ������������� ������ ������������
            exit(0);
        default:
            std::cout << "Invalid choice. Try again.\n";  // ������������ �����
            break;
        }
    }
}

// �������� ���� �������
void Client::run() {
    int choice = 0;
    while (!isAuthenticated_) {
        // ���� �� �����������
        std::cout << "\n1. Register\n2. Login\n3. Exit\nSelect an option: ";
        std::cin >> choice;
        std::cin.ignore(); // ������� ����� �����

        switch (choice) {
        case 1:
            registerUser();  // ��������������
            break;
        case 2:
            loginUser();  // ���������
            break;
        case 3:
            std::cout << "Exiting...\n";  // ������� �� ���������
            isRunning_ = false;  // ������������� ������ ������������
            return;
        default:
            std::cout << "Invalid choice. Try again.\n";  // ������������ �����
            break;
        }
    }
}

// ����������� ������������
void Client::registerUser() {
    std::string login, password, name;
    std::cout << "Enter login: ";
    std::getline(std::cin, login);
    std::cout << "Enter password: ";
    std::getline(std::cin, password);
    std::cout << "Enter name: ";
    std::getline(std::cin, name);

    std::string command = "REGISTER " + login + " " + password + " " + name + "\n";  // ��������� �������
    sendCommand(command);  // ���������� �������

    // �������� ����� �� �������
    std::string response = receiveImmediateResponse();
    if (response.find("REGISTER_SUCCESS") != std::string::npos) {
        std::cout << "Registration successful.\n";  // �������� �����������
    }
    else {
        std::cout << "Registration failed. User may already exist.\n";  // ����������� �� �������
    }
}

// ���� ������������
void Client::loginUser() {
    std::string login, password;
    std::cout << "Enter login: ";
    std::getline(std::cin, login);
    std::cout << "Enter password: ";
    std::getline(std::cin, password);

    std::string command = "LOGIN " + login + " " + password + "\n";  // ��������� �������
    sendCommand(command);  // ���������� �������

    // �������� ����� �� �������
    std::string response = receiveImmediateResponse();

    if (response.find("LOGIN_SUCCESS") != std::string::npos) {
        std::istringstream iss(response);
        std::string status;
        iss >> status >> userId_;  // �������� ������ � ID ������������
        std::string name;
        iss >> name;  // �������� ��� ������������
        std::cout << "Login successful. Welcome, " << name << "!\n";  // ������������ ������������

        isAuthenticated_ = true;  // ������������� ���� �����������

        // ��������� ����� ������������� ���������
        std::thread listenerThread(&Client::listenForMessages, this);
        listenerThread.detach();

        // ��������� � ���� ����� �����������
        authenticatedMenu();
    }
    else {
        std::cout << "Login failed. Check your credentials.\n";  // ����� �� ������
    }
}

// �������� ��������� ����
void Client::sendMessageToAll() {
    std::string message;
    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    std::ostringstream oss;
    oss << "MESSAGE " << userId_ << " 0 " << message << "\n";  // ��������� �������
    std::string command = oss.str();
    sendCommand(command);  // ���������� �������

    // ����� �� ���������
}

// �������� ������� ���������
void Client::sendPrivateMessage() {
    int receiverId;
    std::string message;
    std::cout << "Enter receiver ID: ";
    std::cin >> receiverId;
    std::cin.ignore(); // ������� ����� �����
    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    std::ostringstream oss;
    oss << "MESSAGE " << userId_ << " " << receiverId << " " << message << "\n";  // ��������� �������
    std::string command = oss.str();
    sendCommand(command);  // ���������� �������

    // ����� �� ���������
}

// ����� �� ��������
void Client::logout() {
    std::string command = "LOGOUT " + std::to_string(userId_) + "\n";  // ��������� �������
    sendCommand(command);  // ���������� �������

    // �������� ����� �� �������
    std::string response = receiveImmediateResponse();
    if (response == "LOGOUT_SUCCESS") {
        isAuthenticated_ = false;  // ������� ���� �����������
        userId_ = 0;  // ���������� ID ������������
        std::cout << "Logged out successfully.\n";  // �������� �� �������� ������
    }
    else {
        std::cout << "Logout failed.\n";  // �������� � �������
    }
}

// �������� ������� �� ������
void Client::sendCommand(const std::string& command) {
    int totalSent = 0;
    int commandLength = static_cast<int>(command.size());
    while (totalSent < commandLength) {
        int sent = send(clientSocket_, command.c_str() + totalSent, commandLength - totalSent, 0);  // ���������� ������
        if (sent == SOCKET_ERROR) {
            std::cerr << "send failed: " << WSAGetLastError() << std::endl;  // ������� ������
            break;
        }
        totalSent += sent;  // ����������� ������� ������������ ����
    }
}

// ��������� ������������ ������ �� ������� (������������ ��� ����� � �����������)
std::string Client::receiveImmediateResponse() {
    std::string response;
    std::string tempBuffer;
    char recvbuf[512];
    int recvbuflen = 512;
    auto start = std::chrono::steady_clock::now();  // �������� ����� ������

    while (true) {
        int iResult = recv(clientSocket_, recvbuf, recvbuflen, 0);  // �������� �������� ������
        if (iResult > 0) {
            tempBuffer.append(recvbuf, iResult);  // ��������� ������ � ��������� �����

            size_t pos;
            if ((pos = tempBuffer.find('\n')) != std::string::npos) {  // ���� ����� ���������
                response = tempBuffer.substr(0, pos);  // ��������� ���������
                tempBuffer.erase(0, pos + 1);  // ������� ������������ ��������� �� ������
                // ���� �������� ������, ��������� �� ��� ������ �������������
                if (!tempBuffer.empty()) {
                    leftoverData_ = tempBuffer;
                }
                break;  // ������� �� �����
            }
        }
        else if (iResult == 0) {
            std::cout << "Connection closed by server.\n";  // ������ ������ ����������
            isRunning_ = false;  // ������������� ������
            break;
        }
        else {
            int errCode = WSAGetLastError();  // �������� ��� ������
            if (errCode != WSAEWOULDBLOCK) {
                std::cerr << "recv failed: " << errCode << std::endl;  // ������� ������
                isRunning_ = false;  // ������������� ������
                break;
            }
        }

        auto now = std::chrono::steady_clock::now();  // ������� �����
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 5) {
            std::cout << "Timeout waiting for server response.\n";  // ��������� ����� ��������
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // ���� �������
    }

    return response;  // ���������� ���������� �����
}