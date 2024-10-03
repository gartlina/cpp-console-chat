#include "server.h"
#include <iostream>    // для вывода в консоль
#include <sstream>     // для работы со строковыми потоками
#include <optional>    // для использования std::optional

// конструктор класса Database
Database::Database(const std::string& connection_str) : conn_(nullptr), connection_str_(connection_str) {
    // инициализируем соединение при создании объекта
    connect();
}

// деструктор класса Database
Database::~Database() {
    disconnect();  // отключаемся от базы данных при уничтожении объекта
}

// подключение к базе данных
bool Database::connect() {
    try {
        conn_ = new pqxx::connection(connection_str_);  // создаем новое соединение
        if (conn_->is_open()) {
            std::cout << "Connected to database: " << conn_->dbname() << std::endl;

            // подготавливаем запросы
            conn_->prepare("find_user", "SELECT * FROM users WHERE login = $1");
            conn_->prepare("register_user", "INSERT INTO users (login, password, name) VALUES ($1, $2, $3)");
            conn_->prepare("authenticate_user", "SELECT * FROM users WHERE login = $1");
            conn_->prepare("save_message", "INSERT INTO messages (sender_id, receiver_id, content) VALUES ($1, $2, $3)");
            return true;
        }
        else {
            std::cerr << "Can't open database" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Database connection error: " << e.what() << std::endl;
        return false;
    }
}

// отключение от базы данных
void Database::disconnect() {
    if (conn_) {
        conn_->close();  // закрываем соединение
        delete conn_;    // освобождаем память
        conn_ = nullptr;
    }
}

// регистрация пользователя
bool Database::registerUser(const std::string& login, const std::string& password, const std::string& name) {
    try {
        pqxx::work txn(*conn_);  // начинаем транзакцию

        // проверяем, существует ли уже пользователь с таким логином
        pqxx::result r = txn.exec_prepared("find_user", login);
        if (!r.empty()) {
            return false; // пользователь уже существует
        }

        // хэшируем пароль (используем простое хэширование для примера)
        std::hash<std::string> hasher;
        size_t hashed = hasher(password);
        std::string hashed_password = std::to_string(hashed);

        // вставляем нового пользователя
        txn.exec_prepared("register_user", login, hashed_password, name);
        txn.commit();  // подтверждаем транзакцию
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Register Error: " << e.what() << std::endl;
        return false;
    }
}

// аутентификация пользователя
User Database::authenticateUser(const std::string& login, const std::string& password) {
    User user;
    try {
        pqxx::work txn(*conn_);  // начинаем транзакцию

        pqxx::result r = txn.exec_prepared("authenticate_user", login);
        if (r.empty()) {
            return user; // пользователь не найден
        }

        std::string stored_password = r[0]["password"].as<std::string>();
        bool is_banned = r[0]["is_banned"].as<bool>();
        int id = r[0]["id"].as<int>();
        std::string name = r[0]["name"].as<std::string>();

        // проверяем пароль
        std::hash<std::string> hasher;
        size_t hashed = hasher(password);
        std::string hashed_password = std::to_string(hashed);

        if (hashed_password != stored_password || is_banned) {
            return user; // неверный пароль или пользователь забанен
        }

        user.id = id;
        user.login = login;
        user.name = name;
        user.is_banned = is_banned;
        return user;

    }
    catch (const std::exception& e) {
        std::cerr << "Authenticate Error: " << e.what() << std::endl;
        return user;
    }
}

// сохранить сообщение
void Database::saveMessage(int senderId, int receiverId, const std::string& content, bool is_read) {
    try {
        pqxx::work txn(*conn_);  // начинаем транзакцию
        std::ostringstream query;
        query << "INSERT INTO messages (sender_id, receiver_id, content, is_read) VALUES ("
            << senderId << ", " << receiverId << ", " << txn.quote(content) << ", " << (is_read ? "true" : "false") << ")";
        txn.exec(query.str());  // выполняем запрос
        txn.commit();  // подтверждаем транзакцию
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save message: " << e.what() << std::endl;
    }
}

// получение пользователя по ID
User Database::getUserById(int userId) {
    try {
        pqxx::work txn(*conn_);  // начинаем транзакцию
        std::ostringstream query;

        // выбираем существующие поля
        query << "SELECT id, login, name, is_banned FROM users WHERE id = " << userId;
        pqxx::result r = txn.exec(query.str());

        if (r.size() == 1) {
            User user;
            user.id = r[0]["id"].as<int>();
            user.login = r[0]["login"].as<std::string>();
            user.name = r[0]["name"].as<std::string>();
            user.is_banned = r[0]["is_banned"].as<bool>();
            return user;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Database error: " << e.what() << std::endl;
    }
    return User();  // возвращаем пустого пользователя, если не найдено или ошибка
}