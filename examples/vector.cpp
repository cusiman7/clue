
#include <string_view>
#include <iostream>
#include <clue/clue.h>

struct Args {
    std::vector<int> unlimited = {1, 2}; 
    std::vector<int> atLeastThree = {4, 5, 6};
    std::vector<int> atMostFive = {5, 4, 3, 2, 1};
    std::vector<int> threeToFive = { 3, 5, 7, 9, 11 };
};

void PrintVector(const std::vector<int>& v, const char* name) {
    printf("%s=[", name);  
    for (size_t i = 0; i < v.size(); ++i) {
        const char* fmt = (i == v.size() - 1) ? "%d" : "%d,";
        printf(fmt, v[i]);
    }
    printf("]\n");
}

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("", "Print a message count times.");
    
    cl.Optional(&Args::unlimited, "unlimited", "An unlimited number of numbers");
    cl.Optional<3>(&Args::atLeastThree, "atLeastThree", "At least 3 numbers");
    cl.Optional<0, 5>(&Args::atMostFive, "atMostFive", "At most 5 numbers");
    cl.Optional<3, 5>(&Args::threeToFive, "threeToFive", "3 to 5 numbers");

    auto args = cl.ParseArgs(argc, argv);
    PrintVector(args->unlimited, "unlimited");
    PrintVector(args->atLeastThree, "atLeastThree");
    PrintVector(args->atMostFive, "atMostFive");
    PrintVector(args->threeToFive, "threeToFive");
}
