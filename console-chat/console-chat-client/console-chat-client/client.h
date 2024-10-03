#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <winsock2.h>
#include <atomic>  // ��� ��������� ����������

class Client {
public:
    Client(const std::string& serverIP, int serverPort);  // �����������
    ~Client();  // ����������
    void run();  // ������ �������
    void authenticatedMenu();  // ���� ����� �����������

private:
    void sendCommand(const std::string& command);  // �������� ������� �� ������
    void registerUser();  // �����������
    void loginUser();  // ����
    void sendMessageToAll();  // �������� ��������� ����
    void sendPrivateMessage();  // �������� ������� ���������
    void logout();  // ����� �� ��������
    void listenForMessages();  // ������������� ���������
    std::string receiveImmediateResponse();  // ��������� ������������ ������

    std::string serverIP_;  // IP-����� �������
    int serverPort_;  // ���� �������
    SOCKET clientSocket_;  // ����� �������
    bool isAuthenticated_;  // ���� �����������
    int userId_;  // ID ������������
    std::atomic<bool> isRunning_;  // ���� ������ �������
    std::string leftoverData_;  // ��� �������� ���������� ������
};

#endif // CLIENT_H