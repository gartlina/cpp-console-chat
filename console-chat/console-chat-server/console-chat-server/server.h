#ifndef SERVER_H
#define SERVER_H

#include <string>            // для работы со строками
#include <winsock2.h>        // для работы с сокетами
#include <unordered_map>     // для хеш-таблиц

// класс пользователя
class User {
public:
    int id;                 // идентификатор пользователя
    std::string login;      // логин
    std::string name;       // имя
    SOCKET socket;          // сокет пользователя

    User() : id(0), socket(INVALID_SOCKET) {}
    User(int id_, const std::string& login_, const std::string& name_)
        : id(id_), login(login_), name(name_), socket(INVALID_SOCKET) {}
};

// класс сервера
class Server {
public:
    Server(int port);
    ~Server();
    void run();  // запуск сервера

private:
    void handleClient(SOCKET clientSocket);  // обработка клиента
    void broadcastMessage(const std::string& message);  // рассылка сообщения

    int port_;                                  // порт сервера
    SOCKET listenSocket_;                       // слушающий сокет
    std::unordered_map<SOCKET, User> clients_;  // список клиентов
    std::unordered_map<int, SOCKET> userSockets_; // соответствие userId -> socket
    int userIdCounter_ = 0;                     // счетчик для генерации userId
};

#endif // SERVER_H