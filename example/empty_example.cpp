#include "ini_manager/ini_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
auto main() -> int {
    std::cout << "--- Пример 1: Создание пустого объекта, заполнение и запись в файл ---" << '\n';
    ini::ini_manager config1;
    config1.set_section("Section1");
    config1.set_value("Section1", "Key1", "Value1");
    config1.set_value("Section1", "KeyInt", 42);
    auto write_result1 = config1.write_file("empty_config.ini");
    if (write_result1.has_value()) {
        std::cout << "Конфигурация успешно записана в empty_config.ini" << '\n';
    } else {
        std::cerr << "Ошибка записи в empty_config.ini: " << write_result1.error().message() << '\n';
    }
    std::cout << '\n';

    std::cout << "--- Пример 2: Открытие существующего файла ini, чтение, изменение и запись в него ---" << '\n';
    // Создадим сначала файл для этого примера
    std::ofstream existing_ini("existing.ini");
    if (existing_ini.is_open()) {
        existing_ini << "[Settings]\n";
        existing_ini << "Option1 = OldValue\n";
        existing_ini << "Number = 100\n";
        existing_ini.close();
    }
    auto config_result2 = ini::ini_manager::from_file("existing.ini");
    if (config_result2.has_value()) {
        auto config2 = config_result2.value();
        if (auto option1 = config2.get_value(ini::section{"Settings"}, ini::key{"Option1"})) {
            std::cout << "Option1 до изменения: " << *option1 << '\n';
        }
        config2.set_value("Settings", "Option1", "NewValue");
        config2.set_value("Settings", "NewOption", "AddedOption");
        config2.set_value("Settings", "Number", 200);
        auto write_result2 = config2.write_file("existing.ini"); // Записываем обратно в тот же файл
        if (write_result2.has_value()) {
            std::cout << "Файл existing.ini успешно обновлен." << '\n';
        } else {
            std::cerr << "Ошибка записи в existing.ini: " << write_result2.error().message() << '\n';
        }
    } else {
        std::cerr << "Ошибка открытия existing.ini: " << config_result2.error().message() << '\n';
    }
    std::cout << '\n';

    std::cout << "--- Пример 3: Открытие по потоку, чтение, изменение и запись ---" << '\n';
    std::istringstream ini_stream3("[StreamSection]\nData = StreamData\nCounter = 5\n");
    auto config_result3 = ini::ini_manager::from_stream(ini_stream3);
    if (config_result3.has_value()) {
        auto config3 = config_result3.value();
        if (auto data = config3.get_value(ini::section{"StreamSection"}, ini::key{"Data"})) {
            std::cout << "Data из потока: " << *data << '\n';
        }
        config3.set_value("StreamSection", "Counter", 10);
        config3.set_value("StreamSection", "NewValue", "StreamAdded");
        auto write_result3 = config3.write_file("stream_output.ini");
        if (write_result3.has_value()) {
            std::cout << "Конфигурация из потока записана в stream_output.ini" << '\n';
        } else {
            std::cerr << "Ошибка записи в stream_output.ini: " << write_result3.error().message() << '\n';
        }
    } else {
        std::cerr << "Ошибка загрузки из потока." << '\n';
    }
    std::cout << '\n';

    std::cout << "--- Пример 4: Загрузка другого файла в существующий объект ---" << '\n';
    ini::ini_manager config4; // Создаем пустой объект
    std::cout << "Пустой объект config4 создан." << '\n';
    auto load_result4 = config4.load_file("existing.ini"); // Загружаем файл, созданный в примере 2
    if (load_result4.has_value()) {
        std::cout << "Файл existing.ini успешно загружен в config4." << '\n';
        if (auto app_name = config4.get_value(ini::section{"Settings"}, ini::key{"Number"})) {
            std::cout << "Число из загруженного файла: " << *app_name << '\n';
        }
    } else {
        std::cerr << "Ошибка загрузки existing.ini в config4: " << load_result4.error().message() << '\n';
    }

    return 0;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)