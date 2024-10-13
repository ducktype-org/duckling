#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

enum Week {
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

// Mapa konwersji enum -> string
std::map<Week, std::string> weekToString = {
    {Monday, "Monday"},
    {Tuesday, "Tuesday"},
    {Wednesday, "Wednesday"},
    {Thursday, "Thursday"},
    {Friday, "Friday"},
    {Saturday, "Saturday"},
    {Sunday, "Sunday"}
};

// Mapa konwersji string -> enum
std::map<std::string, Week> stringToWeek = {
    {"Monday", Monday},
    {"Tuesday", Tuesday},
    {"Wednesday", Wednesday},
    {"Thursday", Thursday},
    {"Friday", Friday},
    {"Saturday", Saturday},
    {"Sunday", Sunday}
};

// Funkcja do konwersji enum -> string
std::string enumToString(Week day) {
    return weekToString[day];
}

// Funkcja do konwersji string -> enum
Week stringToEnum(const std::string& dayString) {
    if (stringToWeek.find(dayString) != stringToWeek.end()) {
        return stringToWeek[dayString];
    } else {
        throw std::invalid_argument("Invalid day string");
    }
}

// Klasa reprezentująca plan na dany dzień tygodnia
class WeeklyPlanner {
private:
    std::map<Week, std::vector<std::string>> planner;  // Przechowuje plan zadań dla każdego dnia tygodnia

public:
    // Dodawanie wydarzenia do planu
    void addEvent(Week day, const std::string& event) {
        planner[day].push_back(event);
    }

    // Usuwanie wydarzenia z planu
    void removeEvent(Week day, const std::string& event) {
        auto& events = planner[day];
        for (auto it = events.begin(); it != events.end(); ++it) {
            if (*it == event) {
                events.erase(it);
                std::cout << "Event removed." << "\n";
                return;
            }
        }
        std::cout << "Event not found!" << "\n";
    }

    // Wyświetlanie planu dla danego dnia
    void displayDay(Week day) {
        std::cout << "Plan for " << enumToString(day) << ":" << "\n";
        if (planner[day].empty()) {
            std::cout << "No events." << "\n";
        } else {
            for (const auto& event : planner[day]) {
                std::cout << "- " << event << "\n";
            }
        }
    }

    // Wyświetlanie pełnego tygodniowego planu
    void displayWeek() {
        for (int day = Monday; day <= Sunday; ++day) {
            displayDay(static_cast<Week>(day));
        }
    }

    // Wyczyszczenie planu na dany dzień
    void clearDay(Week day) {
        planner[day].clear();
        std::cout << "Cleared all events for " << enumToString(day) << "\n";
    }
};

// Funkcja interakcji z użytkownikiem
void userInteraction(WeeklyPlanner& planner) {
    while (true) {
        std::cout << "\n--- Weekly Planner ---\n";
        std::cout << "1. Add event\n";
        std::cout << "2. Remove event\n";
        std::cout << "3. Display day\n";
        std::cout << "4. Display week\n";
        std::cout << "5. Clear day\n";
        std::cout << "6. Exit\n";
        std::cout << "Choose an option: ";

        int option;
        std::cin >> option;

        std::string dayString;
        Week day;
        std::string event;

        switch (option) {
            case 1:
                std::cout << "Enter day (e.g., Monday): ";
                std::cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    std::cout << "Enter event: ";
                    std::cin.ignore();
                    std::getline(std::cin, event);
                    planner.addEvent(day, event);
                    std::cout << "Event added." << "\n";
                } catch (const std::invalid_argument& e) {
                    std::cout << "Invalid day!" << "\n";
                }
                break;

            case 2:
                std::cout << "Enter day (e.g., Monday): ";
                std::cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    std::cout << "Enter event to remove: ";
                    std::cin.ignore();
                    std::getline(std::cin, event);
                    planner.removeEvent(day, event);
                } catch (const std::invalid_argument& e) {
                    std::cout << "Invalid day!" << "\n";
                }
                break;

            case 3:
                std::cout << "Enter day (e.g., Monday): ";
                std::cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    planner.displayDay(day);
                } catch (const std::invalid_argument& e) {
                    std::cout << "Invalid day!" << "\n";
                }
                break;

            case 4:
                planner.displayWeek();
                break;

            case 5:
                std::cout << "Enter day (e.g., Monday): ";
                std::cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    planner.clearDay(day);
                } catch (const std::invalid_argument& e) {
                    std::cout << "Invalid day!" << "\n";
                }
                break;

            case 6:
                std::cout << "Exiting planner. Goodbye!" << "\n";
                return;

            default:
                std::cout << "Invalid option!" << "\n";
        }
    }
}

int main() {
    WeeklyPlanner planner;
    userInteraction(planner);
    return 0;
}
