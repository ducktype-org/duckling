#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

using namespace std;

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
map<Week, string> weekToString = {
    {Monday, "Monday"},
    {Tuesday, "Tuesday"},
    {Wednesday, "Wednesday"},
    {Thursday, "Thursday"},
    {Friday, "Friday"},
    {Saturday, "Saturday"},
    {Sunday, "Sunday"}
};

// Mapa konwersji string -> enum
map<string, Week> stringToWeek = {
    {"Monday", Monday},
    {"Tuesday", Tuesday},
    {"Wednesday", Wednesday},
    {"Thursday", Thursday},
    {"Friday", Friday},
    {"Saturday", Saturday},
    {"Sunday", Sunday}
};

// Funkcja do konwersji enum -> string
string enumToString(Week day) {
    return weekToString[day];
}

// Funkcja do konwersji string -> enum
Week stringToEnum(const string& dayString) {
    if (stringToWeek.find(dayString) != stringToWeek.end()) {
        return stringToWeek[dayString];
    } else {
        throw invalid_argument("Invalid day string");
    }
}

// Klasa reprezentująca plan na dany dzień tygodnia
class WeeklyPlanner {
private:
    map<Week, vector<string>> planner;  // Przechowuje plan zadań dla każdego dnia tygodnia

public:
    // Dodawanie wydarzenia do planu
    void addEvent(Week day, const string& event) {
        planner[day].push_back(event);
    }

    // Usuwanie wydarzenia z planu
    void removeEvent(Week day, const string& event) {
        auto& events = planner[day];
        for (auto it = events.begin(); it != events.end(); ++it) {
            if (*it == event) {
                events.erase(it);
                cout << "Event removed." << endl;
                return;
            }
        }
        cout << "Event not found!" << endl;
    }

    // Wyświetlanie planu dla danego dnia
    void displayDay(Week day) {
        cout << "Plan for " << enumToString(day) << ":" << endl;
        if (planner[day].empty()) {
            cout << "No events." << endl;
        } else {
            for (const auto& event : planner[day]) {
                cout << "- " << event << endl;
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
        cout << "Cleared all events for " << enumToString(day) << endl;
    }
};

// Funkcja interakcji z użytkownikiem
void userInteraction(WeeklyPlanner& planner) {
    while (true) {
        cout << "\n--- Weekly Planner ---\n";
        cout << "1. Add event\n";
        cout << "2. Remove event\n";
        cout << "3. Display day\n";
        cout << "4. Display week\n";
        cout << "5. Clear day\n";
        cout << "6. Exit\n";
        cout << "Choose an option: ";

        int option;
        cin >> option;

        string dayString;
        Week day;
        string event;

        switch (option) {
            case 1:
                cout << "Enter day (e.g., Monday): ";
                cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    cout << "Enter event: ";
                    cin.ignore();
                    getline(cin, event);
                    planner.addEvent(day, event);
                    cout << "Event added." << endl;
                } catch (const invalid_argument& e) {
                    cout << "Invalid day!" << endl;
                }
                break;

            case 2:
                cout << "Enter day (e.g., Monday): ";
                cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    cout << "Enter event to remove: ";
                    cin.ignore();
                    getline(cin, event);
                    planner.removeEvent(day, event);
                } catch (const invalid_argument& e) {
                    cout << "Invalid day!" << endl;
                }
                break;

            case 3:
                cout << "Enter day (e.g., Monday): ";
                cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    planner.displayDay(day);
                } catch (const invalid_argument& e) {
                    cout << "Invalid day!" << endl;
                }
                break;

            case 4:
                planner.displayWeek();
                break;

            case 5:
                cout << "Enter day (e.g., Monday): ";
                cin >> dayString;
                try {
                    day = stringToEnum(dayString);
                    planner.clearDay(day);
                } catch (const invalid_argument& e) {
                    cout << "Invalid day!" << endl;
                }
                break;

            case 6:
                cout << "Exiting planner. Goodbye!" << endl;
                return;

            default:
                cout << "Invalid option!" << endl;
        }
    }
}

int main() {
    WeeklyPlanner planner;
    userInteraction(planner);
    return 0;
}
