#ifndef SERVER_H
#define SERVER_H

#include <string>            // ��� ������ �� ��������
#include <winsock2.h>        // ��� ������ � ��������
#include <unordered_map>     // ��� ���-������

// ����� ������������
class User {
public:
    int id;                 // ������������� ������������
    std::string login;      // �����
    std::string name;       // ���
    SOCKET socket;          // ����� ������������

    User() : id(0), socket(INVALID_SOCKET) {}
    User(int id_, const std::string& login_, const std::string& name_)
        : id(id_), login(login_), name(name_), socket(INVALID_SOCKET) {}
};

// ����� �������
class Server {
public:
    Server(int port);
    ~Server();
    void run();  // ������ �������

private:
    void handleClient(SOCKET clientSocket);  // ��������� �������
    void broadcastMessage(const std::string& message);  // �������� ���������

    int port_;                                  // ���� �������
    SOCKET listenSocket_;                       // ��������� �����
    std::unordered_map<SOCKET, User> clients_;  // ������ ��������
    std::unordered_map<int, SOCKET> userSockets_; // ������������ userId -> socket
    int userIdCounter_ = 0;                     // ������� ��� ��������� userId
};

#endif // SERVER_H