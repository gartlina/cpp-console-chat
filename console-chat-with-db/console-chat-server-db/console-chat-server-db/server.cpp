#include "server.h"
#include <iostream>    // для вывода в консоль
#include <thread>      // для работы с потоками
#include <sstream>     // для работы со строковыми потоками

#pragma comment(lib, "Ws2_32.lib")  // подключаем библиотеку сокетов

// конструктор сервера
Server::Server(int port, const std::string& db_connection_str)
    : port_(port), db_(db_connection_str), listenSocket_(INVALID_SOCKET) {
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

    // проверяем подключение к базе данных
    if (!db_.connect()) {
        std::cerr << "Failed to connect to the database" << std::endl;  // выводим ошибку
        closesocket(listenSocket_);  // закрываем сокет
        WSACleanup();  // очищаем ресурсы Winsock
        exit(1);  // выходим из программы
    }
}

// деструктор сервера
Server::~Server() {
    // закрываем все клиентские сокеты
    for (auto& pair : clients_) {
        closesocket(pair.first);  // закрываем сокет клиента
    }
    // закрываем слушающий сокет
    closesocket(listenSocket_);
    // отключаемся от базы данных
    db_.disconnect();
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
                    std::string login, password, name;
                    iss >> login >> password >> name;
                    bool success = db_.registerUser(login, password, name);  // пытаемся зарегистрировать пользователя
                    std::string response = success ? "REGISTER_SUCCESS\n" : "REGISTER_FAIL\n";
                    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);  // отправляем ответ
                    std::cout << "Registration attempt: " << login << " - " << (success ? "Success" : "Fail") << std::endl;
                }
                else if (command == "LOGIN") {
                    std::string login, password;
                    iss >> login >> password;
                    currentUser = db_.authenticateUser(login, password);  // пытаемся аутентифицировать пользователя
                    if (currentUser.id != 0) {
                        isAuthenticated = true;  // устанавливаем флаг аутентификации
                        clients_[clientSocket] = currentUser;  // добавляем клиента в список
                        userSockets_[currentUser.id] = clientSocket;  // сохраняем сокет пользователя

                        std::ostringstream oss;
                        oss << "LOGIN_SUCCESS " << currentUser.id << " " << currentUser.name << "\n";
                        std::string response = oss.str();
                        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);  // отправляем ответ
                        std::cout << "User logged in: " << login << std::endl;

                        // отправляем непрочитанные сообщения
                        try {
                            pqxx::work txn(*db_.getConnection());
                            std::ostringstream query;
                            query << "SELECT * FROM messages WHERE receiver_id = " << currentUser.id << " AND is_read = false";
                            pqxx::result messages = txn.exec(query.str());

                            for (const auto& row : messages) {
                                int senderId = row["sender_id"].as<int>();
                                int receiverId = row["receiver_id"].as<int>();
                                std::string content = row["content"].as<std::string>();

                                // получаем имена отправителя и получателя
                                std::string senderName, receiverName;
                                User senderUser = db_.getUserById(senderId);
                                if (senderUser.id != 0) {
                                    senderName = senderUser.name;
                                }
                                else {
                                    senderName = "Unknown";
                                }

                                if (receiverId == 0) {
                                    receiverName = "All";
                                }
                                else {
                                    User receiverUser = db_.getUserById(receiverId);
                                    if (receiverUser.id != 0) {
                                        receiverName = receiverUser.name;
                                    }
                                    else {
                                        receiverName = "Unknown";
                                    }
                                }

                                // формируем сообщение с именами
                                std::ostringstream formattedMsg;
                                formattedMsg << "NEW_MESSAGE " << senderId << " " << receiverId << " "
                                    << senderName << " " << receiverName << " " << content << "\n";

                                send(clientSocket, formattedMsg.str().c_str(), static_cast<int>(formattedMsg.str().size()), 0);  // отправляем сообщение

                                // отмечаем сообщение как прочитанное
                                std::ostringstream updateQuery;
                                updateQuery << "UPDATE messages SET is_read = true WHERE id = " << row["id"].as<int>();
                                txn.exec(updateQuery.str());
                            }

                            txn.commit();  // подтверждаем транзакцию
                        }
                        catch (const std::exception& e) {
                            std::cerr << "Error sending unread messages: " << e.what() << std::endl;
                        }
                    }
                    else {
                        std::string response = "LOGIN_FAIL\n";
                        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);  // отправляем ответ
                        std::cout << "Failed login attempt: " << login << std::endl;
                    }
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
                        // получаем имя получателя из базы данных
                        User receiverUser = db_.getUserById(receiverId);
                        if (receiverUser.id != 0) {
                            receiverName = receiverUser.name;
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

                        // при необходимости можно сохранить широковещательные сообщения
                    }
                    else {
                        // личное сообщение
                        auto it = userSockets_.find(receiverId);
                        if (it != userSockets_.end()) {
                            // пользователь онлайн
                            send(it->second, formattedMsg.str().c_str(), static_cast<int>(formattedMsg.str().size()), 0);  // отправляем сообщение
                            std::cout << "Private message from " << senderName << " to " << receiverName << ": " << content << std::endl;

                            // сохраняем сообщение как прочитанное
                            db_.saveMessage(senderId, receiverId, content, true);
                        }
                        else {
                            // пользователь офлайн
                            // сохраняем сообщение как непрочитанное
                            db_.saveMessage(senderId, receiverId, content, false);
                            std::cout << "User " << receiverName << " is not connected. Message saved for later delivery." << std::endl;
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