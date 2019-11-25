# C.L.U.E. Command Line User Experience

[![](https://github.com/cusiman7/clue/workflows/C/C++%20CI/badge.svg)](https://github.com/cusiman7/clue/actions?query=workflow%3A%22C%2FC%2B%2B+CI%22)

Clue is an easy to use C++ 17 command line parser for programmers and their command line users.

## A Simple Example

```
#include <clue/clue.h>

struct Args {
    std::string message = "Hello Clue!";
    int count = 1;
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("simple", "Print a message count times.");
    
    cl.Optional(&Args::count, "count", "Number of times to print the message");
    cl.Positional(&Args::message, "message", "A message to print");

    auto args = cl.ParseArgs(argc, argv);

    for (int i = 0; i < args->count; ++i) {
        std::cout << args->message << "\n";
    }
}
``` 

## What Your Users See

What your users see and feel when interacting with your command line tool is even more important than the language you used to write it in or the specific features its command line libraries supported.
Clue puts command line users first with its type aware command line argument parsing and powerful help message generation.
Arguments are annotated with types, labeled as required or optional when appropriate, and formatted to sensible line lengths for humans to read.

For example, a user running `./simple -h` from the example above sees:

```
usage: simple [-count <int>] [message <string>]

Print a message count times.

    -count <int>: Number of times to print the message (Default: 1)

    message <string>: A message to print (Default: Hello Clue!)
```

## Technical Features

* Detailed program "usage" string generation based on names, descriptions, types, and default values of arguments
* Help output formatted to sensible line lengths for humans to read
* Positional and Optional arguments
* Expressive type-aware parsing and error messages for your users
  * Data types: `int`, `float`, `double`, `bool`, `string`, and `string_view`
  * `std::array<T, N>` container of any of the above data types (except bool)
  * `std::vector<T>` container of any of the above data types (except bool)
  * structs composed of any of the above types
  * Full custom types support for construction of your own types right from the command line (see below for examples)
* Configurable
  * Don't exit early on an error
  * Ignore unrecognized arguments
  * Disable auto-help generation and build your own
  * Disable defaults printing
  * Mark any or all arguments as required
* Header only
* [Builds](https://github.com/cusiman7/clue/actions?query=workflow%3A%22C%2FC%2B%2B+CI%22) for Ubuntu 18 (GCC 7), MacOS (Clang), and Windows (MSVC)

## More Examples

There are a few examples in the `examples` directory, demonstrating many of CLUE's features.
Below are a couple of them.

### Vectors for Variadic Arguments

Vectors can be used to specify arguments that may take any number of arguments. 
To restrict the number of arguments (lower bound or upper bound inclusively) you're willing to accpet 
pass `size_t` template arguments to `Optional<lowerBound, upperBound>` or `Positional<lowerBound, upperBound>`.

```
struct Args {
    std::vector<int> unlimited = {1, 2}; 
    std::vector<float> atLeastThree = {4, 5, 6};
    std::vector<double> atMostFive = {5, 4, 3, 2, 1};
    std::vector<std::string> threeToFive = {};
};

int main(int argc, char** argv) {
    clue::CommandLine<Args> cl("vector example", "");
    
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
```

### Argument Struct with Custom Types

Custom types are supported by passing the type and the type's constructor arguments as additional `clue::CommandLine` template arguments.
For example a Vec3 representing a vector in 3D space with three floats would look like `Vec3(float, float, float)`.
Aggregates are of course supported as well, in which case you should pass the types in proper aggregate initialization order.
Note, defaults help cannot be auto-generated for custom types due to the current limits of C++.

```
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

// Tell CLUE about Vec and Repeat and how to construct them
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
```

If you don't wish to have CLUE build an `Args` struct for you but you still wish to use custom types, just pass `std::monostate` as CLUE's first tempalte paramter:

    clue::CommandLine<std::monostate, Vec3(float, float, float>>

## Roadmap
* Enum for "choices" (from\_string helper required though?)
* Better support for aliases and short options 
* Europe friendliness 10,12456
* Large number friendliness 1,000,000.12

