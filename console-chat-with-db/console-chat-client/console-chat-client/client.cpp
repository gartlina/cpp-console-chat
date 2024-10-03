#include "client.h"
#include <iostream>      // для ввода-вывода
#include <sstream>       // для работы со строковыми потоками
#include <ws2tcpip.h>    // для функций сокетов
#include <string>        // для работы со строками
#include <thread>        // для использования потоков
#include <chrono>        // для работы со временем
#include <atomic>        // для атомарных переменных

#pragma comment(lib, "Ws2_32.lib")  // подключаем библиотеку сокетов

// конструктор клиента
Client::Client(const std::string& serverIP, int serverPort)
    : serverIP_(serverIP), serverPort_(serverPort),
    isAuthenticated_(false), userId_(0), isRunning_(true) {
    // инициализируем Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  // запрашиваем версию 2.2
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;  // выводим ошибку
        exit(1);  // выходим из программы
    }

    // создаем сокет
    clientSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // создаем TCP-сокет
    if (clientSocket_ == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;  // выводим ошибку
        WSACleanup();  // очищаем ресурсы Winsock
        exit(1);  // выходим из программы
    }

    // устанавливаем сокет в неблокирующий режим
    u_long mode = 1;  // 1 означает неблокирующий режим
    ioctlsocket(clientSocket_, FIONBIO, &mode);  // устанавливаем режим сокета

    // задаем адрес сервера
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;  // используем IPv4
    if (InetPtonA(AF_INET, serverIP_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported" << std::endl;  // выводим ошибку
        closesocket(clientSocket_);  // закрываем сокет
        WSACleanup();  // очищаем ресурсы Winsock
        exit(1);  // выходим из программы
    }
    serverAddr.sin_port = htons(serverPort_);  // устанавливаем порт сервера

    // подключаемся к серверу
    iResult = connect(clientSocket_, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        int err = WSAGetLastError();  // получаем код ошибки
        if (err != WSAEWOULDBLOCK && err != WSAEISCONN) {  // если ошибка не связана с неблокирующим сокетом
            std::cerr << "Unable to connect to server: " << err << std::endl;  // выводим ошибку
            closesocket(clientSocket_);  // закрываем сокет
            WSACleanup();  // очищаем ресурсы Winsock
            exit(1);  // выходим из программы
        }
    }

    std::cout << "Connected to server.\n";  // сообщаем о подключении
}

// деструктор клиента
Client::~Client() {
    isRunning_ = false;  // сигнализируем потоку прослушивания остановиться
    closesocket(clientSocket_);  // закрываем сокет
    WSACleanup();  // очищаем ресурсы Winsock
}

// функция потока для прослушивания сообщений
void Client::listenForMessages() {
    char recvbuf[512];  // буфер для приема данных
    int recvbuflen = 512;  // размер буфера
    std::string recvBuffer = leftoverData_;  // начинаем с любых оставшихся данных

    while (isRunning_) {  // пока клиент работает
        int iResult = recv(clientSocket_, recvbuf, recvbuflen, 0);  // пытаемся получить данные

        if (iResult > 0) {
            recvBuffer.append(recvbuf, iResult);  // добавляем полученные данные в буфер

            size_t pos;
            while ((pos = recvBuffer.find('\n')) != std::string::npos) {  // ищем полный сообщение
                std::string message = recvBuffer.substr(0, pos);  // извлекаем сообщение
                recvBuffer.erase(0, pos + 1);  // удаляем обработанное сообщение из буфера

                // парсим сообщение
                std::istringstream iss(message);
                std::string command;
                iss >> command;

                if (command == "NEW_MESSAGE") {
                    int senderId, receiverId;
                    std::string senderName, receiverName;
                    std::string content;
                    iss >> senderId >> receiverId >> senderName >> receiverName;  // читаем идентификаторы и имена
                    std::getline(iss, content);  // получаем содержимое сообщения
                    if (!content.empty() && content[0] == ' ') {
                        content.erase(0, 1); // убираем начальный пробел
                    }

                    if (receiverId == 0) {
                        // сообщение для всех
                        std::cout << "\nNew message: [ALL] '" << senderName << "': '" << content << "'\n";
                    }
                    else if (receiverId == userId_) {
                        // личное сообщение этому пользователю
                        std::cout << "\nNew message: [PM] '" << senderName << "' -> 'You': '" << content << "'\n";
                    }
                    else {
                        // сообщение не предназначено этому пользователю
                        // можно опционально обработать или игнорировать
                    }
                    std::cout << "\n";  // добавляем пустую строку для разделения
                }
                else {
                    std::cout << "\nReceived message: " << message << std::endl;  // выводим полученное сообщение
                }
            }
        }
        else if (iResult == 0) {
            std::cout << "Connection closed by server.\n";  // сервер закрыл соединение
            isRunning_ = false;  // останавливаем клиент
        }
        else {
            int errCode = WSAGetLastError();  // получаем код ошибки
            if (errCode != WSAEWOULDBLOCK) {
                std::cerr << "recv failed: " << errCode << std::endl;  // выводим ошибку
                isRunning_ = false;  // останавливаем клиент при ошибке
            }
            else {
                // данных нет, спим, чтобы избежать интенсивного опроса
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
}

// меню после авторизации
void Client::authenticatedMenu() {
    int choice = 0;
    while (isAuthenticated_) {
        std::cout << "\n1. Send to all\n2. Send private message\n3. Logout\n4. Exit\nSelect an option: ";
        std::cin >> choice;
        std::cin.ignore(); // очищаем буфер ввода

        switch (choice) {
        case 1:
            sendMessageToAll();  // отправляем сообщение всем
            break;
        case 2:
            sendPrivateMessage();  // отправляем личное сообщение
            break;
        case 3:
            logout();  // выходим из аккаунта
            return;  // возвращаемся после выхода
        case 4:
            std::cout << "Exiting...\n";  // выходим из программы
            isRunning_ = false;  // сигнализируем потоку остановиться
            exit(0);
        default:
            std::cout << "Invalid choice. Try again.\n";  // неправильный выбор
            break;
        }
    }
}

// основной цикл клиента
void Client::run() {
    int choice = 0;
    while (!isAuthenticated_) {
        // меню до авторизации
        std::cout << "\n1. Register\n2. Login\n3. Exit\nSelect an option: ";
        std::cin >> choice;
        std::cin.ignore(); // очищаем буфер ввода

        switch (choice) {
        case 1:
            registerUser();  // регистрируемся
            break;
        case 2:
            loginUser();  // логинимся
            break;
        case 3:
            std::cout << "Exiting...\n";  // выходим из программы
            isRunning_ = false;  // сигнализируем потоку остановиться
            return;
        default:
            std::cout << "Invalid choice. Try again.\n";  // неправильный выбор
            break;
        }
    }
}

// регистрация пользователя
void Client::registerUser() {
    std::string login, password, name;
    std::cout << "Enter login: ";
    std::getline(std::cin, login);
    std::cout << "Enter password: ";
    std::getline(std::cin, password);
    std::cout << "Enter name: ";
    std::getline(std::cin, name);

    std::string command = "REGISTER " + login + " " + password + " " + name + "\n";  // формируем команду
    sendCommand(command);  // отправляем команду

    // получаем ответ от сервера
    std::string response = receiveImmediateResponse();
    if (response.find("REGISTER_SUCCESS") != std::string::npos) {
        std::cout << "Registration successful.\n";  // успешная регистрация
    }
    else {
        std::cout << "Registration failed. User may already exist.\n";  // регистрация не удалась
    }
}

// вход пользователя
void Client::loginUser() {
    std::string login, password;
    std::cout << "Enter login: ";
    std::getline(std::cin, login);
    std::cout << "Enter password: ";
    std::getline(std::cin, password);

    std::string command = "LOGIN " + login + " " + password + "\n";  // формируем команду
    sendCommand(command);  // отправляем команду

    // получаем ответ от сервера
    std::string response = receiveImmediateResponse();

    if (response.find("LOGIN_SUCCESS") != std::string::npos) {
        std::istringstream iss(response);
        std::string status;
        iss >> status >> userId_;  // получаем статус и ID пользователя
        std::string name;
        iss >> name;  // получаем имя пользователя
        std::cout << "Login successful. Welcome, " << name << "!\n";  // приветствуем пользователя

        isAuthenticated_ = true;  // устанавливаем флаг авторизации

        // запускаем поток прослушивания сообщений
        std::thread listenerThread(&Client::listenForMessages, this);
        listenerThread.detach();

        // переходим в меню после авторизации
        authenticatedMenu();
    }
    else {
        std::cout << "Login failed. Check your credentials.\n";  // логин не удался
    }
}

// отправка сообщения всем
void Client::sendMessageToAll() {
    std::string message;
    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    std::ostringstream oss;
    oss << "MESSAGE " << userId_ << " 0 " << message << "\n";  // формируем команду
    std::string command = oss.str();
    sendCommand(command);  // отправляем команду

    // ответ не ожидается
}

// отправка личного сообщения
void Client::sendPrivateMessage() {
    int receiverId;
    std::string message;
    std::cout << "Enter receiver ID: ";
    std::cin >> receiverId;
    std::cin.ignore(); // очищаем буфер ввода
    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    std::ostringstream oss;
    oss << "MESSAGE " << userId_ << " " << receiverId << " " << message << "\n";  // формируем команду
    std::string command = oss.str();
    sendCommand(command);  // отправляем команду

    // ответ не ожидается
}

// выход из аккаунта
void Client::logout() {
    std::string command = "LOGOUT " + std::to_string(userId_) + "\n";  // формируем команду
    sendCommand(command);  // отправляем команду

    // получаем ответ от сервера
    std::string response = receiveImmediateResponse();
    if (response == "LOGOUT_SUCCESS") {
        isAuthenticated_ = false;  // снимаем флаг авторизации
        userId_ = 0;  // сбрасываем ID пользователя
        std::cout << "Logged out successfully.\n";  // сообщаем об успешном выходе
    }
    else {
        std::cout << "Logout failed.\n";  // сообщаем о неудаче
    }
}

// отправка команды на сервер
void Client::sendCommand(const std::string& command) {
    int totalSent = 0;
    int commandLength = static_cast<int>(command.size());
    while (totalSent < commandLength) {
        int sent = send(clientSocket_, command.c_str() + totalSent, commandLength - totalSent, 0);  // отправляем данные
        if (sent == SOCKET_ERROR) {
            std::cerr << "send failed: " << WSAGetLastError() << std::endl;  // выводим ошибку
            break;
        }
        totalSent += sent;  // увеличиваем счетчик отправленных байт
    }
}

// получение немедленного ответа от сервера (используется при входе и регистрации)
std::string Client::receiveImmediateResponse() {
    std::string response;
    std::string tempBuffer;
    char recvbuf[512];
    int recvbuflen = 512;
    auto start = std::chrono::steady_clock::now();  // отмечаем время начала

    while (true) {
        int iResult = recv(clientSocket_, recvbuf, recvbuflen, 0);  // пытаемся получить данные
        if (iResult > 0) {
            tempBuffer.append(recvbuf, iResult);  // добавляем данные в временный буфер

            size_t pos;
            if ((pos = tempBuffer.find('\n')) != std::string::npos) {  // ищем конец сообщения
                response = tempBuffer.substr(0, pos);  // извлекаем сообщение
                tempBuffer.erase(0, pos + 1);  // удаляем обработанное сообщение из буфера
                // если остались данные, сохраняем их для потока прослушивания
                if (!tempBuffer.empty()) {
                    leftoverData_ = tempBuffer;
                }
                break;  // выходим из цикла
            }
        }
        else if (iResult == 0) {
            std::cout << "Connection closed by server.\n";  // сервер закрыл соединение
            isRunning_ = false;  // останавливаем клиент
            break;
        }
        else {
            int errCode = WSAGetLastError();  // получаем код ошибки
            if (errCode != WSAEWOULDBLOCK) {
                std::cerr << "recv failed: " << errCode << std::endl;  // выводим ошибку
                isRunning_ = false;  // останавливаем клиент
                break;
            }
        }

        auto now = std::chrono::steady_clock::now();  // текущее время
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 5) {
            std::cout << "Timeout waiting for server response.\n";  // превышено время ожидания
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // спим немного
    }

    return response;  // возвращаем полученный ответ
}