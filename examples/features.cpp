#include <clue/clue.h>

struct Args {
    bool hello = false;
    int i = 0;
    float f = 0;
    double d = 0;
    std::string s = "default";
    std::string_view sv;
    std::array<int, 3> veci = {1, 2, 3};
    std::array<float, 3> vecf = {1, 2, 3};
    std::array<double, 4> quat = {0, 0, 0, 1};
};

int main(int argc, char** argv) {
    bool hello = false;
    int i = 0;
    float f = 0;
    double d = 0;
    std::string s;
    std::array<int, 3> veci = {0, 0, 0};
    std::array<float, 3> vecf = {0, 0, 0};
    std::array<double, 3> vecd = {0, 0, 0};
    std::string_view str_view;

    clue::CommandLine<Args> cl("Features", "This is a test program for testing command line parsing and all the different ways one might want to parse things.\n\n"
                         "Our tenets for CommandLine are:\n"
                         "    1. Great for the command line user\n"
                         "    2. Great for the command line programmer\n"
                         "    3. Understandable for us to program and maintain");
    cl.Optional(&Args::hello, "hello", "say hello");
    cl.Optional(&Args::veci, "veci", "3 int point");
    cl.Optional(&Args::vecf, "vecf", "3 float point");
    cl.Optional(&Args::quat, "quat", "A quaternion");
    cl.Optional(&Args::i, "int", "The description of this arg is just way to long to be useful but we're using it here to test if line breaking is working as expected for variable descriptions. Does it?");
    cl.Optional(&Args::f, "float", "A float");
    cl.Optional(&Args::d, "double", "A double");
    cl.Optional(&Args::s, "name", "A name");
    cl.Optional(&Args::sv, "name_view", "Also a name");
    cl.Optional(&veci, "raw_veci", "A \"raw veci\"");

    cl.Optional(&hello, "raw_hello", "Another way of saying hello, but to a bool, not a member");
    cl.Optional(&i, "raw_int", "Another way of passing an integer, also not a member");
    cl.Optional(&f, "raw_float", "Floats that are raw");
    cl.Optional(&d, "raw_double", "Double");
    cl.Optional(&s, "raw_string", "A string value");
    cl.Optional(&vecf, "raw_vecf", "A 3 float vector");
    cl.Optional(&vecd, "raw_vecd", "A 3 double vector");
    cl.Optional(&str_view, "raw_strview", "Another string view to finish it all off");

    auto [args, success] = cl.ParseArgs(argc, argv);
    if (!success) {
        printf("Arg parsing failed\n");
    }
    printf("Args: \n");
    printf("  hello = %s\n", args.hello ? "true" : "false");
    printf("  i = %d\n", args.i);
    printf("  f = %f\n", args.f);
    printf("  d = %f\n", args.d);
    printf("  veci[1] = %d\n", args.veci[1]);
    printf("  s = %s\n", args.s.c_str());
    printf("  sv = %s\n", args.sv.data());
    
    printf("hello = %s\n", hello ? "true" : "false");
    printf("i = %d\n", i);
    printf("f = %f\n", f);
    printf("d = %f\n", d);
    printf("s = %s\n", s.c_str());
    printf("str_view = %s\n", str_view.data());
    for (uint32_t i = 0; i < veci.size(); ++i) {
        printf("  veci[%d] = %d", i, veci[i]);
    }
    printf("\n");
    for (uint32_t i = 0; i < vecf.size(); ++i) {
        printf("  vecf[%d] = %f", i, vecf[i]);
    }
    printf("\n");
    for (uint32_t i = 0; i < vecd.size(); ++i) {
        printf("  vecd[%d] = %f", i, vecd[i]);
    }
    printf("\n");

    return 0;
}
