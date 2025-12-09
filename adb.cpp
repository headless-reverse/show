#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <limits>
#include <atomic>
#include <sstream>
#include <cstdio>

using json = nlohmann::json;

std::atomic<bool> monitoring_running(false);

struct TriggerRule {
    std::string script;
    std::vector<std::string> args;
    bool auth_required = false;
    int delay_sec = 0;
};

std::map<std::string, std::vector<TriggerRule>> loadTriggers(const std::string& config_file) {
    std::map<std::string, std::vector<TriggerRule>> triggers;
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[!] Plik '" << config_file << "' nie znaleziony. Zaczynam z pusta konfiguracja." << std::endl;
        return triggers;
    }
    try {
        json j = json::parse(file);
        for (auto& [serial, actions] : j.items()) {
            std::vector<TriggerRule> rules;
            for (const auto& action : actions) {
                TriggerRule rule;
                rule.script = action.value("action_script", "");
                rule.auth_required = action.value("auth_required", false);
                rule.delay_sec = action.value("delay_sec", 0);
                if (action.contains("action_args") && action["action_args"].is_array()) {
                    for (const auto& arg : action["action_args"]) {
                        rule.args.push_back(arg.get<std::string>());
                    }
                }
                rules.push_back(rule);
            }
            triggers[serial] = rules;
        }
        std::cout << "[✓] Ladowanie autotriggers z '" << config_file << "' powiodlo sie." << std::endl;
    } catch (json::parse_error& e) {
        std::cerr << "[!] Blad parsowania pliku '" << config_file << "': " << e.what() << std::endl;
    }
    return triggers;
}

void saveTriggers(const std::string& config_file, const std::map<std::string, std::vector<TriggerRule>>& triggers) {
    json j;
    for (const auto& pair : triggers) {
        json actions_array = json::array();
        for (const auto& rule : pair.second) {
            json action;
            action["action_script"] = rule.script;
            if (!rule.args.empty()) {
                action["action_args"] = rule.args;
            }
            action["auth_required"] = rule.auth_required;
            action["delay_sec"] = rule.delay_sec;
            actions_array.push_back(action);
        }
        j[pair.first] = actions_array;
    }
    std::ofstream file(config_file);
    if (file.is_open()) {
        file << std::setw(4) << j << std::endl;
        std::cout << "[✓] Zapisano konfiguracje do pliku '" << config_file << "'." << std::endl;
    } else {
        std::cerr << "[!] Blad: Nie mozna zapisac pliku '" << config_file << "'." << std::endl;
    }
}

void executeScriptWithDelay(const TriggerRule& rule) {
    if (rule.delay_sec > 0) {
        std::cout << "  [•] Opóźniam wykonanie akcji '" << rule.script << "' o " << rule.delay_sec << " sekund." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(rule.delay_sec));
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "[!] Blad: fork() nie powiodl sie." << std::endl;
        return;
    }

    // Proces potomny
    if (pid == 0) {
        std::vector<char*> argv_vec;
        argv_vec.push_back(const_cast<char*>(rule.script.c_str()));
        for (const auto& arg : rule.args) {
            argv_vec.push_back(const_cast<char*>(arg.c_str()));
        }
        argv_vec.push_back(nullptr);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(rule.script.c_str(), argv_vec.data());
        perror("execvp");
        exit(1);
    }
    // Proces macierzysty
    else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            std::cout << "  [✓] Akcja '" << rule.script << "' zakonczona sukcesem." << std::endl;
        } else {
            std::cout << "  [X] Akcja '" << rule.script << "' zakonczona bledem. Kod wyjscia: " << WEXITSTATUS(status) << std::endl;
        }
    }
}

void monitorAdbDevices(const std::string& config_file) {
    std::cout << "[•] Uruchamianie monitorowania urządzeń ADB. (Ctrl+C, aby zatrzymać)" << std::endl;
    std::map<std::string, bool> connected_devices;

    while (monitoring_running) {
        FILE* pipe = popen("adb devices", "r");
        if (!pipe) {
            std::cerr << "[!] Błąd: Nie można uruchomić polecenia 'adb devices'." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        char buffer[128];
        std::string result = "";
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        pclose(pipe);

        std::map<std::string, bool> new_devices;
        std::stringstream ss(result);
        std::string line;

        std::getline(ss, line);

        while (std::getline(ss, line)) {
            if (line.empty() || line.find("device") == std::string::npos) {
                continue;
            }
            std::string serial = line.substr(0, line.find('\t'));
            new_devices[serial] = true;

            if (connected_devices.find(serial) == connected_devices.end()) {
                std::cout << "\n[+] Wykryto nowe urządzenie ADB: " << serial << std::endl;

                std::map<std::string, std::vector<TriggerRule>> triggers = loadTriggers(config_file);
                if (triggers.count(serial)) {
                    std::cout << "  [•] Znaleziono " << triggers[serial].size() << " akcji dla tego urządzenia." << std::endl;
                    for (const auto& rule : triggers[serial]) {
                        executeScriptWithDelay(rule);
                    }
                } else {
                    std::cout << "  [•] Brak skonfigurowanych akcji dla " << serial << "." << std::endl;
                }
            }
        }
        connected_devices = new_devices;

        std::this_thread::sleep_for(std::chrono::seconds(1)); // interwał
    }
    std::cout << "[✓] Zatrzymano monitorowanie ADB." << std::endl;
}

void connectWirelessDevice() {
    std::string ip_address;
    std::string port_number;

    std::cout << "\nPodaj adres IP: ";
    std::cin >> ip_address;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << "Podaj numer portu: ";
    std::cin >> port_number;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string command = "adb connect " + ip_address + ":" + port_number;
    std::cout << "[•] Uruchamiam: " << command << std::endl;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "[!] Błąd: Nie można uruchomić polecenia 'adb connect'." << std::endl;
        return;
    }

    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != nullptr) {
            result += buffer;
        }
    }
    pclose(pipe);

    std::cout << "Wynik: " << result << std::endl;
    std::cout << "[✓] Spróbowano połączyć z urządzeniem bezprzewodowym." << std::endl;
}

void showMenu(const std::string& config_file) {
    std::map<std::string, std::vector<TriggerRule>> triggers = loadTriggers(config_file);
    std::string choice;
    while (true) {
        std::cout << "\n### autotriggers ###" << std::endl;
        std::cout << "1. Wyswietl biezace autotriggers" << std::endl;
        std::cout << "2. Dodaj nowy autotrigger" << std::endl;
        std::cout << "3. Usun autotrigger" << std::endl;
        std::cout << "4. Rozpocznij monitorowanie ADB" << std::endl;
        std::cout << "4a. Dodaj urządzenie bezprzewodowe (IP:Port)" << std::endl;
        std::cout << "5. Zatrzymaj monitorowanie" << std::endl;
        std::cout << "6. Zapisz i Wyjdz" << std::endl;
        std::cout << "7. Wyjdz bez zapisu" << std::endl;
        std::cout << "\nWybierz opcje: ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == "1") {
            if (triggers.empty()) {
                std::cout << "\nBrak autotriggers w konfiguracji." << std::endl;
            } else {
                std::cout << "\n### Biezace autotriggers ###" << std::endl;
                for (const auto& pair : triggers) {
                    std::cout << "ID urzadzenia (serial ADB): " << pair.first << std::endl;
                    for (size_t i = 0; i < pair.second.size(); ++i) {
                        std::cout << "  - Akcja #" << i + 1 << ":" << std::endl;
                        std::cout << "    - Skrypt: " << pair.second[i].script << std::endl;
                        if (!pair.second[i].args.empty()) {
                            std::cout << "    - Argumenty: ";
                            for (const auto& arg : pair.second[i].args) {
                                std::cout << arg << " ";
                            }
                            std::cout << std::endl;
                        }
                        std::cout << "    - Wymaga autoryzacji: " << (pair.second[i].auth_required ? "Tak" : "Nie") << std::endl;
                        std::cout << "    - Opóźnienie: " << pair.second[i].delay_sec << "s" << std::endl;
                    }
                }
            }
        } else if (choice == "2") {
            std::string serial;
            std::cout << "Podaj numer seryjny urzadzenia (np. 'emulator-5554'): ";
            std::cin >> serial;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            TriggerRule new_rule;
            std::cout << "Podaj sciezke do skryptu: ";
            std::getline(std::cin, new_rule.script);

            std::cout << "Podaj argumenty skryptu, oddzielajac je spacjami (puste, jesli brak): ";
            std::string args_input;
            std::getline(std::cin, args_input);
            if (!args_input.empty()) {
                std::string arg;
                std::stringstream ss(args_input);
                while(ss >> arg) {
                    new_rule.args.push_back(arg);
                }
            }

            std::cout << "Czy skrypt wymaga autoryzacji (tak/nie)? ";
            std::string auth_input;
            std::cin >> auth_input;
            new_rule.auth_required = (auth_input == "tak" || auth_input == "t");

            std::cout << "Podaj opóźnienie w sekundach (0, jesli brak): ";
            std::cin >> new_rule.delay_sec;
            
            triggers[serial].push_back(new_rule);
            std::cout << "\n[✓] Dodano nowy autotrigger dla urzadzenia " << serial << "." << std::endl;

        } else if (choice == "3") {
            std::string serial_to_remove;
            std::cout << "Podaj numer seryjny urzadzenia do usuniecia: ";
            std::cin >> serial_to_remove;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            if (triggers.count(serial_to_remove)) {
                triggers.erase(serial_to_remove);
                std::cout << "\n[✓] Usunieto wszystkie autotrigger dla urzadzenia " << serial_to_remove << "." << std::endl;
            } else {
                std::cout << "\n[!] Nie znaleziono urzadzenia " << serial_to_remove << "." << std::endl;
            }
        } else if (choice == "4") {
            if (monitoring_running) {
                std::cout << "\n[!] Monitorowanie juz dziala." << std::endl;
            } else {
                monitoring_running = true;
                std::thread monitor_thread(monitorAdbDevices, config_file);
                monitor_thread.detach();
            }
        } else if (choice == "4a") {
            connectWirelessDevice();
        } else if (choice == "5") {
            if (!monitoring_running) {
                std::cout << "\n[!] Monitorowanie juz jest wylaczone." << std::endl;
            } else {
                monitoring_running = false;
                std::cout << "\n[✓] Wyslano sygnal do wylaczenia monitorowania. Prosze czekac..." << std::endl;
            }
        } else if (choice == "6") {
            saveTriggers(config_file, triggers);
            std::cout << "\n[✓] Zapisano i zakonczono." << std::endl;
            break;
        } else if (choice == "7") {
            std::cout << "\n[✓] Zakonczono bez zapisu." << std::endl;
            break;
        } else {
            std::cout << "\n[!] Nieprawidlowa opcja. Sprobuj ponownie." << std::endl;
        }
    }
}

void usage(const std::string& program_name) {
    std::cout << "Uzycie: " << program_name << " [OPCJE]" << std::endl;
    std::cout << "Opcje:" << std::endl;
    std::cout << "  --config <plik>      Uzyj niestandardowego pliku konfiguracyjnego triggers.json" << std::endl;
    std::cout << "  --daemon             Uruchamia program w trybie monitorowania w tle bez menu." << std::endl;
    std::cout << "  --help               Wyswietla te informacje." << std::endl;
    std::cout << "\nDomyslnie program uruchamia tryb interaktywnego menu." << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file = "triggers.json";
    bool run_as_daemon = false;
    bool show_help = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "[!] Blad: brak nazwy pliku po '--config'." << std::endl;
                usage(argv[0]);
                return 1;
            }
        } else if (std::string(argv[i]) == "--daemon") {
            run_as_daemon = true;
        } else if (std::string(argv[i]) == "--help") {
            show_help = true;
        }
    }
    
    if (show_help) {
        usage(argv[0]);
        return 0;
    }
    
    if (run_as_daemon) {
        monitoring_running = true;
        monitorAdbDevices(config_file);
    } else {
        showMenu(config_file);
    }

    return 0;
}
