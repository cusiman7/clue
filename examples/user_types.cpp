#include <iostream>
#include <string_view>
#include <clue/clue.h>

struct Vec3 {
    float x = 1;
    float y = 2;
    float z = 3;
};

struct Repeat {
    std::string_view phrase = "Hello Clue!";
    int i = 3;
};

struct Args {
    Vec3 vec;
    Repeat repeat;
};

using CommandLine = clue::CommandLine<Args, 
    Vec3(float, float, float),
    Repeat(std::string_view, int)>;

int main(int argc, char** argv) {
    CommandLine cl("User Types");

    cl.Optional(&Args::vec, "vec", "A 3 value Vector (Default: 1.0, 2.0, 3.0)");
    cl.Optional(&Args::repeat, "repeat", "Repeat a phrase N times (Default: 3, Hello Clue!)");

    auto args = cl.ParseArgs(argc, argv);
    printf("Vec3(%f, %f, %f)\n", args->vec.x, args->vec.y, args->vec.z);
    for (int i = 0; i < args->repeat.i; ++i) {
        printf("%s\n", args->repeat.phrase.data());
    }
}
