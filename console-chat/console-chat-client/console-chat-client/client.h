#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <winsock2.h>
#include <atomic>  // для атомарных переменных

class Client {
public:
    Client(const std::string& serverIP, int serverPort);  // конструктор
    ~Client();  // деструктор
    void run();  // запуск клиента
    void authenticatedMenu();  // меню после авторизации

private:
    void sendCommand(const std::string& command);  // отправка команды на сервер
    void registerUser();  // регистрация
    void loginUser();  // вход
    void sendMessageToAll();  // отправка сообщения всем
    void sendPrivateMessage();  // отправка личного сообщения
    void logout();  // выход из аккаунта
    void listenForMessages();  // прослушивание сообщений
    std::string receiveImmediateResponse();  // получение немедленного ответа

    std::string serverIP_;  // IP-адрес сервера
    int serverPort_;  // порт сервера
    SOCKET clientSocket_;  // сокет клиента
    bool isAuthenticated_;  // флаг авторизации
    int userId_;  // ID пользователя
    std::atomic<bool> isRunning_;  // флаг работы клиента
    std::string leftoverData_;  // для хранения оставшихся данных
};

#endif // CLIENT_H