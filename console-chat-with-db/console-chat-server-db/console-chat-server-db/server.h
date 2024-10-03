#ifndef SERVER_H
#define SERVER_H

#include <string>            // ��� ������ �� ��������
#include <winsock2.h>        // ��� ������ � ��������
#include <unordered_map>     // ��� ���-������
#include <vector>            // ��� ������������� ��������
#include <pqxx/pqxx>         // ��� ������ � PostgreSQL

// ����� ������������
class User {
public:
    int id;                 // ������������� ������������
    std::string login;      // �����
    std::string name;       // ���
    bool is_banned;         // ���� ����
    SOCKET socket;          // ����� ������������

    User() : id(0), is_banned(false), socket(INVALID_SOCKET) {}
    User(int id_, const std::string& login_, const std::string& name_, bool is_banned_)
        : id(id_), login(login_), name(name_), is_banned(is_banned_), socket(INVALID_SOCKET) {}
};

// ����� ���� ������
class Database {
public:
    Database(const std::string& connection_str);
    ~Database();

    bool connect();    // ����������� � ���� ������
    void disconnect(); // ���������� �� ���� ������

    bool registerUser(const std::string& login, const std::string& password, const std::string& name);  // ����������� ������������
    User authenticateUser(const std::string& login, const std::string& password);  // �������������� ������������
    void saveMessage(int senderId, int receiverId, const std::string& content, bool is_read = false);  // ���������� ���������
    User getUserById(int userId);  // ��������� ������������ �� ID
    pqxx::connection* getConnection() {
        return conn_;  // ���������� ��������� �� ����������
    }

private:
    pqxx::connection* conn_;        // ���������� � ����� ������
    std::string connection_str_;    // ������ �����������
};

// ����� �������
class Server {
public:
    Server(int port, const std::string& db_connection_str);
    ~Server();
    void run();  // ������ �������

private:
    void handleClient(SOCKET clientSocket);  // ��������� �������
    void broadcastMessage(const std::string& message);  // �������� ���������

    int port_;                                  // ���� �������
    SOCKET listenSocket_;                       // ��������� �����
    std::unordered_map<SOCKET, User> clients_;  // ������ ��������
    std::unordered_map<int, SOCKET> userSockets_; // ������������ userId -> socket
    Database db_;                               // ������ ���� ������
};

#endif // SERVER_H