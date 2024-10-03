#include "server.h"

int main() {
    int port = 1234;  // порт сервера

    Server server(port);  // создаем сервер
    server.run();  // запускаем сервер

    return 0;  // завершаем программу
}