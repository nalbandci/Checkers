#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
public:
    // Конструктор по умолчанию: при создании объекта сразу загружает конфигурацию из файла
    Config()
    {
        reload();
    }

    // Функция reload() перезагружает конфигурационные данные из файла settings.json
    // Используется для обновления настроек во время выполнения программы
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;  // Парсинг JSON из файла в объект json
        fin.close();
    }

    // Оператор () позволяет получать значения настроек в удобной форме
    // Принимает два строковых параметра: раздел и имя настройки
    // Пример использования: config("Bot", "WhiteBotLevel") вернет значение уровня белого бота
    auto operator()(const string& setting_dir, const string& setting_name) const
    {
        return config[setting_dir][setting_name];
    }

private:
    json config;  // Внутренний объект для хранения конфигурационных данных в формате JSON
};