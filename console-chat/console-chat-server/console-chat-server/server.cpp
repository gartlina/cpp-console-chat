#include "server.h"
#include <iostream>    // для вывода в консоль
#include <thread>      // для работы с потоками
#include <sstream>     // для работы со строковыми потоками

#pragma comment(lib, "Ws2_32.lib")  // подключаем библиотеку сокетов

// конструктор сервера
Server::Server(int port)
    : port_(port), listenSocket_(INVALID_SOCKET) {
    // инициализируем Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  // запрашиваем версию 2.2
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;  // выводим ошибку
        exit(1);  // выходим из программы
    }

    // создаем слушающий сокет
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // создаем TCP-сокет
    if (listenSocket_ == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;  // выводим ошибку
        WSACleanup();  // очищаем ресурсы Winsock
        exit(1);  // выходим из программы
    }

    // настраиваем адрес для прослушивания
    sockaddr_in service;
    service.sin_family = AF_INET;  // используем IPv4
    service.sin_addr.s_addr = INADDR_ANY;  // принимаем соединения с любых адресов
    service.sin_port = htons(port_);  // устанавливаем порт

    // связываем сокет с адресом
    if (bind(listenSocket_, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << std::endl;  // выводим ошибку
        closesocket(listenSocket_);  // закрываем сокет
        WSACleanup();  // очищаем ресурсы Winsock
        exit(1);  // выходим из программы
    }

    // начинаем прослушивание входящих соединений
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << std::endl;  // выводим ошибку
        closesocket(listenSocket_);  // закрываем сокет
        WSACleanup();  // очищаем ресурсы Winsock
        exit(1);  // выходим из программы
    }

    std::cout << "Server started on port " << port_ << std::endl;  // сообщаем о запуске сервера
}

// деструктор сервера
Server::~Server() {
    // закрываем все клиентские сокеты
    for (auto& pair : clients_) {
        closesocket(pair.first);  // закрываем сокет клиента
    }
    // закрываем слушающий сокет
    closesocket(listenSocket_);
    // очищаем ресурсы Winsock
    WSACleanup();
}

// запуск сервера
void Server::run() {
    while (true) {
        sockaddr_in clientInfo;  // информация о клиенте
        int clientInfoSize = sizeof(clientInfo);
        // принимаем входящее соединение
        SOCKET clientSocket = accept(listenSocket_, (SOCKADDR*)&clientInfo, &clientInfoSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;  // выводим ошибку
            continue;  // переходим к следующей итерации
        }

        std::cout << "New connection accepted: " << clientSocket << std::endl;  // сообщаем о новом соединении

        // обрабатываем клиента в отдельном потоке
        std::thread(&Server::handleClient, this, clientSocket).detach();  // запускаем поток и отсоединяем его
    }
}

// обработка клиента
void Server::handleClient(SOCKET clientSocket) {
    char recvbuf[512];  // буфер для приема данных
    int recvbuflen = 512;  // размер буфера
    std::string leftover;  // для хранения неполных данных

    User currentUser;  // текущий пользователь
    bool isAuthenticated = false;  // флаг аутентификации

    while (true) {
        int iResult = recv(clientSocket, recvbuf, recvbuflen, 0);  // пытаемся получить данные от клиента
        if (iResult > 0) {
            leftover += std::string(recvbuf, iResult);  // добавляем полученные данные

            size_t pos;
            while ((pos = leftover.find('\n')) != std::string::npos) {  // ищем полный запрос
                std::string line = leftover.substr(0, pos);  // извлекаем строку запроса
                leftover.erase(0, pos + 1);  // удаляем обработанный запрос из буфера

                std::istringstream iss(line);
                std::string command;
                iss >> command;

                if (command == "REGISTER") {
                    // регистрация без базы данных: всегда успех
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
                    // вход без базы данных: генерируем пользователя
                    std::string login, password;
                    iss >> login >> password;
                    currentUser.id = ++userIdCounter_;
                    currentUser.login = login;
                    currentUser.name = login;  // используем логин как имя
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

                    // получаем имена отправителя и получателя
                    std::string senderName = currentUser.name;
                    std::string receiverName;

                    if (receiverId == 0) {
                        receiverName = "All";
                    }
                    else {
                        // ищем получателя среди подключенных пользователей
                        auto it = clients_.find(userSockets_[receiverId]);
                        if (it != clients_.end()) {
                            receiverName = it->second.name;
                        }
                        else {
                            receiverName = "Unknown";
                        }
                    }

                    // формируем сообщение с именами
                    std::ostringstream formattedMsg;
                    formattedMsg << "NEW_MESSAGE " << senderId << " " << receiverId << " "
                        << senderName << " " << receiverName << " " << content << "\n";

                    if (receiverId == 0) {
                        // широковещательное сообщение
                        broadcastMessage(formattedMsg.str());  // рассылаем всем
                        std::cout << "Broadcast message from " << senderName << ": " << content << std::endl;
                    }
                    else {
                        // личное сообщение
                        auto it = userSockets_.find(receiverId);
                        if (it != userSockets_.end()) {
                            // пользователь онлайн
                            send(it->second, formattedMsg.str().c_str(), static_cast<int>(formattedMsg.str().size()), 0);  // отправляем сообщение
                            std::cout << "Private message from " << senderName << " to " << receiverName << ": " << content << std::endl;
                        }
                        else {
                            // пользователь офлайн
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

                    // удаляем пользователя из списков
                    clients_.erase(clientSocket);
                    userSockets_.erase(senderId);

                    std::cout << "User logged out: " << senderId << std::endl;

                    // отправляем подтверждение
                    std::string response = "LOGOUT_SUCCESS\n";
                    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);

                    // завершаем соединение
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
            break;  // клиент закрыл соединение
        }
        else {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            break;  // ошибка получения данных
        }
    }

    // очистка ресурсов
    closesocket(clientSocket);  // закрываем сокет клиента
    clients_.erase(clientSocket);  // удаляем клиента из списка
    for (auto it = userSockets_.begin(); it != userSockets_.end();) {
        if (it->second == clientSocket) {
            it = userSockets_.erase(it);  // удаляем запись
        }
        else {
            ++it;
        }
    }
}

// рассылка сообщения всем подключенным клиентам
void Server::broadcastMessage(const std::string& message) {
    for (const auto& pair : clients_) {
        send(pair.first, message.c_str(), static_cast<int>(message.size()), 0);  // отправляем сообщение каждому клиенту
    }
}