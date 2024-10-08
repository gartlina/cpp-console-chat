# C++ Console Chat

Это простое консольное чат-приложение, состоящее из клиента и сервера, написанных на C++. Существует две версии сервера:

- **С базой данных (PostgreSQL):** Позволяет сохранять пользователей и сообщения в базе данных. Поддерживает офлайн-сообщения и аутентификацию пользователей.
- **Без базы данных:** Упрощенная версия без сохранения данных. Пользователи регистрируются и аутентифицируются на лету, сообщения не сохраняются.

## Содержание

- [Особенности](#особенности)
- [Требования](#требования)
- [Сборка и запуск](#сборка-и-запуск)
  - [Версия с базой данных](#версия-с-базой-данных)
  - [Версия без базы данных](#версия-без-базы-данных)
- [Использование](#использование)
- [Структура проекта](#структура-проекта)
- [Примечания](#примечания)

## Особенности

### Клиент

- Регистрация нового пользователя.
- Вход существующего пользователя.
- Отправка сообщений всем пользователям.
- Отправка личных сообщений по ID пользователя.
- Выход из аккаунта.
- Прослушивание новых сообщений в реальном времени.

### Сервер с базой данных

- Использует PostgreSQL для хранения пользователей и сообщений.
- Поддерживает офлайн-сообщения (непрочитанные сообщения доставляются при следующем входе пользователя).
- Аутентификация и регистрация пользователей.
- Сохранение сообщений в базе данных.

### Сервер без базы данных

- Сообщения не сохраняются и не доставляются офлайн-пользователям.
- Генерация уникальных идентификаторов пользователей на лету.

## Требования

- **Компилятор C++:** Поддерживающий стандарт C++17 или выше.
- **Библиотека Winsock2:** Для работы с сетевыми сокетами (Windows). Уже встроена в ОС Windows, скачивать и устанавливать не надо.
- **PostgreSQL и libpqxx (только для версии с базой данных):**
  - Установленный сервер PostgreSQL.
  - Библиотека `libpqxx` для работы с PostgreSQL в C++.

## Сборка и запуск

### Версия с базой данных

1. **Настройка базы данных:**

   - Установите PostgreSQL и создайте новую базу данных, например, `chat`.
   - Создайте необходимые таблицы:

     ```sql
     CREATE TABLE users (
       id SERIAL PRIMARY KEY,
       login VARCHAR(50) UNIQUE NOT NULL,
       password VARCHAR(255) NOT NULL,
       name VARCHAR(100) NOT NULL,
       is_banned BOOLEAN DEFAULT FALSE
     );

     CREATE TABLE messages (
       id SERIAL PRIMARY KEY,
       sender_id INTEGER REFERENCES users(id),
       receiver_id INTEGER, -- 0 для сообщений всем
       content TEXT NOT NULL,
       is_read BOOLEAN DEFAULT FALSE
     );
     ```
   - В `console-chat-server-db.cpp` введите данные для подключения к БД:
       ```cpp
       std::string dbConnectionStr = "host=localhost port=5432 dbname=chat user=username password=password";  // строка подключения к БД
       ```
       - где username - имя пользователя от БД
       - password - пароль от пользователя БД
2. **Установка `libpqxx`:**

   - Скачайте и установите `libpqxx` соответствующей версии для вашей системы.
   - Нужно для подключения к БД PostgreSQL.

3. **Запуск:**
   - Запустите сначала сервер.
   - Потом запустите клиента.

### Версия без базы данных

1. **Сборка клиента:**

  - Клиент тот же самый.

2. **Запуск:**
   
   - Запустите сначала сервер.
   - Потом запустите клиента.

## Использование

1. **Регистрация:**

   - Выберите опцию `1` для регистрации.
   - Введите логин, пароль и имя.

2. **Вход:**

   - Выберите опцию `2` для входа.
   - Введите логин и пароль.

3. **Отправка сообщений:**

   - После входа вы можете:
     - Отправить сообщение всем (опция `1`).
     - Отправить личное сообщение (опция `2`), указав `ID` получателя.

4. **Выход:**

   - Выберите опцию `3` для выхода из аккаунта.

5. **Закрытие клиента:**

   - Выберите опцию `4` для выхода из приложения.

## Структура проекта

- `client.h` и `client.cpp` - исходный код клиента.
- `server.h` и `server.cpp` - исходный код сервера.
- `database.cpp` - реализация взаимодействия с базой данных (только для версии с БД).

## Примечания

- **Версия с базой данных** обеспечивает более полный функционал, включая сохранение пользователей и сообщений.
- **Версия без базы данных** предназначена для демонстрационных целей или случаев, когда установка базы данных затруднительна.
- **Порт и адрес:** По умолчанию сервер слушает на порту `1234`. Вы можете изменить это в коде.
- **Клиент и сервер должны находиться в одной сети или быть настроены соответствующим образом для связи друг с другом.**
