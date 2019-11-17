
#include <string_view>
#include <iostream>
#include <clue/clue.h>

struct Args {
    std::vector<int> unlimited = {1, 2}; 
    std::vector<float> atLeastThree = {4, 5, 6};
    std::vector<double> atMostFive = {5, 4, 3, 2, 1};
    std::vector<std::string> threeToFive = {};
};

template <typename T>
void PrintVector(const std::vector<T>& v, std::string_view name) {
    std::cout << name << "=[";
    auto size = v.size();
    for (size_t i = 0; i < size; ++i) {
        std::cout << v[i];
        if (i != size - 1) { 
            std::cout << ",";
        }
    }
    std::cout << "]\n";
}

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("", "Print a message count times.");
    
    cl.Optional(&Args::unlimited, "unlimited", "An unlimited number of arguments");
    cl.Optional<3>(&Args::atLeastThree, "atLeastThree", "At least 3 arguments");
    cl.Optional<0, 5>(&Args::atMostFive, "atMostFive", "At most 5 arguments");
    cl.Positional<3, 5>(&Args::threeToFive, "threeToFive", "3 to 5 arguments");

    auto args = cl.ParseArgs(argc, argv);
    PrintVector(args->unlimited, "unlimited");
    PrintVector(args->atLeastThree, "atLeastThree");
    PrintVector(args->atMostFive, "atMostFive");
    PrintVector(args->threeToFive, "threeToFive");
}
