#include <cl/cl.h>

struct Args {
    bool hello = false;
    int i = 0;
    float f = 0;
    double d = 0;
    std::string s = "default";
    std::string_view sv;
    std::array<int, 3> veci = {1, 2, 3};
    std::array<float, 3> vecf = {1, 2, 3};
    std::array<double, 4> quat;
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

    rac::CommandLine<Args> cl("Features", "This is a test program for testing command line parsing and all the different ways one might want to parse things.\n\n"
                         "Our tenets for CommandLine are:\n"
                         "    1. Great for the command line user\n"
                         "    2. Great for the command line programmer\n"
                         "    3. Understandable for us to program and maintain");
    cl.Add("hello", &Args::hello, "say hello");
    cl.Add("veci", &Args::veci, "3 int point");
    cl.Add("vecf", &Args::vecf, "3 float point");
    cl.Add("quat", &Args::quat, "A quaternion");
    cl.Add("int", &Args::i, "The description of this arg is just way to long to be useful but we're using it here to test if line breaking is working as expected for variable descriptions. Does it?");
    cl.Add("float", &Args::f, "A float");
    cl.Add("double", &Args::d, "A double");
    cl.Add("name", &Args::s, "A name");
    cl.Add("name_view", &Args::sv, "Also a name");
    cl.Add("raw_veci", &veci, "A \"raw veci\"");


    cl.Add("raw_hello", &hello, "Another way of saying hello, but to a bool, not a member");
    cl.Add("raw_int", &i, "Another way of passing an integer, also not a member");
    cl.Add("raw_float", &f, "Floats that are raw");
    cl.Add("raw_double", &d, "Double");
    cl.Add("raw_string", &s, "A string value");
    cl.Add("raw_vecf", &vecf, "A 3 float vector");
    cl.Add("raw_vecd", &vecd, "A 3 double vector");
    cl.Add("raw_strview", &str_view, "Another string view to finish it all off");

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
