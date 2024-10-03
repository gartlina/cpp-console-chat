#include "server.h"
#include <iostream>    // ��� ������ � �������
#include <sstream>     // ��� ������ �� ���������� ��������
#include <optional>    // ��� ������������� std::optional

// ����������� ������ Database
Database::Database(const std::string& connection_str) : conn_(nullptr), connection_str_(connection_str) {
    // �������������� ���������� ��� �������� �������
    connect();
}

// ���������� ������ Database
Database::~Database() {
    disconnect();  // ����������� �� ���� ������ ��� ����������� �������
}

// ����������� � ���� ������
bool Database::connect() {
    try {
        conn_ = new pqxx::connection(connection_str_);  // ������� ����� ����������
        if (conn_->is_open()) {
            std::cout << "Connected to database: " << conn_->dbname() << std::endl;

            // �������������� �������
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

// ���������� �� ���� ������
void Database::disconnect() {
    if (conn_) {
        conn_->close();  // ��������� ����������
        delete conn_;    // ����������� ������
        conn_ = nullptr;
    }
}

// ����������� ������������
bool Database::registerUser(const std::string& login, const std::string& password, const std::string& name) {
    try {
        pqxx::work txn(*conn_);  // �������� ����������

        // ���������, ���������� �� ��� ������������ � ����� �������
        pqxx::result r = txn.exec_prepared("find_user", login);
        if (!r.empty()) {
            return false; // ������������ ��� ����������
        }

        // �������� ������ (���������� ������� ����������� ��� �������)
        std::hash<std::string> hasher;
        size_t hashed = hasher(password);
        std::string hashed_password = std::to_string(hashed);

        // ��������� ������ ������������
        txn.exec_prepared("register_user", login, hashed_password, name);
        txn.commit();  // ������������ ����������
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Register Error: " << e.what() << std::endl;
        return false;
    }
}

// �������������� ������������
User Database::authenticateUser(const std::string& login, const std::string& password) {
    User user;
    try {
        pqxx::work txn(*conn_);  // �������� ����������

        pqxx::result r = txn.exec_prepared("authenticate_user", login);
        if (r.empty()) {
            return user; // ������������ �� ������
        }

        std::string stored_password = r[0]["password"].as<std::string>();
        bool is_banned = r[0]["is_banned"].as<bool>();
        int id = r[0]["id"].as<int>();
        std::string name = r[0]["name"].as<std::string>();

        // ��������� ������
        std::hash<std::string> hasher;
        size_t hashed = hasher(password);
        std::string hashed_password = std::to_string(hashed);

        if (hashed_password != stored_password || is_banned) {
            return user; // �������� ������ ��� ������������ �������
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

// ��������� ���������
void Database::saveMessage(int senderId, int receiverId, const std::string& content, bool is_read) {
    try {
        pqxx::work txn(*conn_);  // �������� ����������
        std::ostringstream query;
        query << "INSERT INTO messages (sender_id, receiver_id, content, is_read) VALUES ("
            << senderId << ", " << receiverId << ", " << txn.quote(content) << ", " << (is_read ? "true" : "false") << ")";
        txn.exec(query.str());  // ��������� ������
        txn.commit();  // ������������ ����������
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save message: " << e.what() << std::endl;
    }
}

// ��������� ������������ �� ID
User Database::getUserById(int userId) {
    try {
        pqxx::work txn(*conn_);  // �������� ����������
        std::ostringstream query;

        // �������� ������������ ����
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
    return User();  // ���������� ������� ������������, ���� �� ������� ��� ������
}