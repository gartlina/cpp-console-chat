#include "client.h"

int main() {
    // IP-адрес и порт сервера
    std::string serverIP = "127.0.0.1";  // локальный адрес
    int serverPort = 1234;  // порт сервера

    // создаем и запускаем клиента
    Client client(serverIP, serverPort);
    client.run();

    return 0;  // завершение программы
}