// https://www.programiz.com/cpp-programming/oop#example1

#include <iostream>
#include <string>

class Car {
  public:

    // class data
    std::string brand, model;
    int mileage = 0;

    // class function to drive the car
    void drive(int distance) {
        mileage += distance;
    }
    
    // class function to print variables
    void show_data() {
        std::cout << "Brand: " << brand << std::endl;
        std::cout << "Model: " << model << std::endl;
        std::cout << "Distance driven: " << mileage << " miles" << std::endl;
    }
};

int main() {
    
    // create an object of Car class
    Car my_car;

    // initialize variables of my_car
    my_car.brand = "Honda";
    my_car.model = "Accord";
    my_car.drive(50);

    // display object variables
    my_car.show_data();
}
