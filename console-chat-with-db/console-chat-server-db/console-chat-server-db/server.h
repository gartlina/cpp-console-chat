#ifndef SERVER_H
#define SERVER_H

#include <string>            // для работы со строками
#include <winsock2.h>        // для работы с сокетами
#include <unordered_map>     // для хеш-таблиц
#include <vector>            // для использования векторов
#include <pqxx/pqxx>         // для работы с PostgreSQL

// класс пользователя
class User {
public:
    int id;                 // идентификатор пользователя
    std::string login;      // логин
    std::string name;       // имя
    bool is_banned;         // флаг бана
    SOCKET socket;          // сокет пользователя

    User() : id(0), is_banned(false), socket(INVALID_SOCKET) {}
    User(int id_, const std::string& login_, const std::string& name_, bool is_banned_)
        : id(id_), login(login_), name(name_), is_banned(is_banned_), socket(INVALID_SOCKET) {}
};

// класс базы данных
class Database {
public:
    Database(const std::string& connection_str);
    ~Database();

    bool connect();    // подключение к базе данных
    void disconnect(); // отключение от базы данных

    bool registerUser(const std::string& login, const std::string& password, const std::string& name);  // регистрация пользователя
    User authenticateUser(const std::string& login, const std::string& password);  // аутентификация пользователя
    void saveMessage(int senderId, int receiverId, const std::string& content, bool is_read = false);  // сохранение сообщения
    User getUserById(int userId);  // получение пользователя по ID
    pqxx::connection* getConnection() {
        return conn_;  // возвращаем указатель на соединение
    }

private:
    pqxx::connection* conn_;        // соединение с базой данных
    std::string connection_str_;    // строка подключения
};

// класс сервера
class Server {
public:
    Server(int port, const std::string& db_connection_str);
    ~Server();
    void run();  // запуск сервера

private:
    void handleClient(SOCKET clientSocket);  // обработка клиента
    void broadcastMessage(const std::string& message);  // рассылка сообщения

    int port_;                                  // порт сервера
    SOCKET listenSocket_;                       // слушающий сокет
    std::unordered_map<SOCKET, User> clients_;  // список клиентов
    std::unordered_map<int, SOCKET> userSockets_; // соответствие userId -> socket
    Database db_;                               // объект базы данных
};

#endif // SERVER_H