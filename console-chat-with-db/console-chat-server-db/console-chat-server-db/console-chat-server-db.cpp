#include "server.h"

int main() {
    std::string dbConnectionStr = "host=localhost port=5432 dbname=chat user=username password=password";  // строка подключения к БД
    int port = 1234;  // порт сервера

    Server server(port, dbConnectionStr);  // создаем сервер
    server.run();  // запускаем сервер

    return 0;  // завершаем программу
}