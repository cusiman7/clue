
#include <array>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <functional>
#include <charconv>
#include <limits>
#include <type_traits>

#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// TODO:
// vector<type>
// Enum for "choices" (from_string helper required though?)
// arg descriptions
// proper line breaking

#if defined(__GNUC__) || defined(__clang)
#define PRINTF_LIKE(a, b) __attribute__((format(printf, (a), (b))))
#else
#define PRINTF_LIKE(a, b)
#endif

inline void ReportError(const char* fmt, va_list vaList) {
    vfprintf(stderr, fmt, vaList);
}

PRINTF_LIKE(1, 2)
inline void ReportError(const char* fmt, ...) {
    va_list vaList;
    va_start(vaList, fmt);
    ReportError(fmt, vaList);
    va_end(vaList);
}

PRINTF_LIKE(2, 3)
inline void Assert(bool assertion, const char* fmt, ...) {
    if (!assertion) {
        va_list vaList;
        va_start(vaList, fmt);
        ReportError(fmt, vaList);
        va_end(vaList);
        std::abort();
    }
}
    
template <typename T>
struct ParseResults {
    T args;
    bool success;
};

template <typename T>
struct Array {
    T* begin;
    T* end;
};

struct StringBuilder {
    StringBuilder() {
        Grow(4096);
    }

    ~StringBuilder() {
        free(buf_);
    }

    PRINTF_LIKE(2, 3)
    void Append(const char* fmt, ...) {
        va_list vaList;
        va_list copy;
        va_start(vaList, fmt);
        va_copy(copy, vaList);
        int len = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);
        if (lineLen_ + len > 80) {
            lineLen_ = 0;
            AppendAndGrow("\n    ");
        }
        AppendAndGrow(len, fmt, vaList);
        va_end(vaList);
    }

    std::string_view GetStringView() const {
        return {buf_, static_cast<size_t>(i_)};
    }
private:
    void Grow(int newSize) {
        bufSize_ = std::max(bufSize_*2, newSize);
        buf_ = static_cast<char*>(realloc(buf_, sizeof(char) * bufSize_));
    }

    PRINTF_LIKE(2, 3)
    void AppendAndGrow(const char* fmt, ...) {
        va_list vaList;
        va_start(vaList, fmt);
        AppendAndGrow(fmt, vaList);
        va_end(vaList);
    }

    void AppendAndGrow(const char* fmt, va_list vaList) {
        va_list copy;
        va_copy(copy, vaList);
        int len = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);
        AppendAndGrow(len, fmt, vaList);
    }
    
    void AppendAndGrow(int len, const char* fmt, va_list vaList) {
        if (i_ + len + 1 >= bufSize_) {
            Grow(i_ + len +  1); // plus 1 for nullchar
        }
        assert(i_ + len < bufSize_);
        int added = vsnprintf(buf_+i_, bufSize_-i_, fmt, vaList);
        lineLen_ += added;
        i_ += added;
    }

    int i_ = 0;
    int lineLen_ = 0;
    char* buf_ = nullptr;
    int bufSize_ = 0;
};

template <typename T=std::monostate>
struct CommandLine {
    using ParseResults = ParseResults<T>;

    template <typename U>
    using ArrayMemberVariant = std::variant<std::array<U, 1> T::*, std::array<U, 2> T::*, std::array<U, 3> T::*, std::array<U, 4> T::*, std::array<U, 5> T::*,
         std::array<U, 6> T::*, std::array<U, 7> T::*, std::array<U, 8> T::*, std::array<U, 9> T::*, std::array<U, 10> T::*>;
    using IntVariant = std::variant<int*, int T::*, Array<int>, ArrayMemberVariant<int>>;
    using FloatVariant = std::variant<float*, float T::*, double*, double T::*, Array<float>, Array<double>, ArrayMemberVariant<float>, ArrayMemberVariant<double>>;
    using BoolVariant = std::variant<bool*, bool T::*>;
    using StringVariant = std::variant<std::string_view*, std::string_view T::*, std::string*, std::string T::*>;

    using ArgumentVariant = std::variant<IntVariant, FloatVariant, BoolVariant, StringVariant>;

    template <typename A, typename ParseFunc>
    bool ParseArray(A&& arrayPtr, ParseFunc&& parseFunc) {
        auto* curr = arrayPtr->begin;
        auto* end = arrayPtr->end;
        while (curr != end) {
            auto [v, success] = parseFunc();
            if (!success) {
                return false;
            }
            *curr = v;
            curr++;
        }
        return true;
    }

    template <typename AMVP, typename ParseFunc> 
    bool ParseArrayMemberVariant(T& t, AMVP&& arrayMemberVariantPtr, ParseFunc&& parseFunc) {
        return std::visit([&](auto& arrayMember) -> bool {
            auto* curr = (t.*arrayMember).begin();
            auto* end = (t.*arrayMember).end();
            while (curr != end) {
                auto [v, success] = parseFunc();
                if (!success) {
                    return false;
                }
                *curr = v;
                curr++;
            }
            return true;
        }, *arrayMemberVariantPtr);
    }

    CommandLine(std::string_view description = "") : description_(description) {
    }

    struct Arg {
        template <typename V>
        Arg(std::string_view name, V* value, std::string_view description = "") 
            : name(name), argument(value), description(description) {
        }
        template <typename MemberT>
        Arg(std::string_view name, MemberT T::* member, std::string_view description = "") 
            : name(name), argument(member), description(description) {
        }
        template <typename U, size_t N>
        Arg(std::string_view name, std::array<U, N>* array, std::string_view description = "")
            : name(name), argument(Array<U>{array->begin(), array->end()}), description(description) {
        }

        std::string_view name;
        ArgumentVariant argument;

        std::string_view description;
    };

    template <typename ...Args>
    void Add(std::string_view name, Args&&... args) {
        for (const auto& arg : args_) {
            Assert(name != arg.name, "Name \"%.*s\" already registered\n", static_cast<int>(name.size()), name.data());
        }
        args_.emplace_back(name, std::forward<Args&&>(args)...);
    }

    void PrintUsageAndExit(int argc, char** argv) const {

        StringBuilder stringBuilder;

        // Default construct a T, for type info of ArrayMembers and default values
        const static T t = {};

        // build usage line
        stringBuilder.Append("usage: %s", argv[0]);
        for (const auto& arg : args_) {
            if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
                if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    stringBuilder.Append(" [-%s <int[%zu]>]", arg.name.data(), size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                    size_t size = std::visit([](auto arrayMember) {
                        return (t.*arrayMember).size();
                    }, *arrayMemberVariant);
                    stringBuilder.Append(" [-%s <int[%zu]>]", arg.name.data(), size);
                }
            } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
                if (auto arrayPtr = std::get_if<Array<float>>(floatVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    stringBuilder.Append(" [-%s <float[%zu]>]", arg.name.data(), size);
                } else if (auto arrayPtr = std::get_if<Array<double>>(floatVariant)) {
                    size_t size = arrayPtr->end - arrayPtr->begin;
                    stringBuilder.Append(" [-%s <double[%zu]>]", arg.name.data(), size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<float>>(floatVariant)) {
                    size_t size = std::visit([](auto arrayMember) {
                        return (t.*arrayMember).size();
                    }, *arrayMemberVariant);
                    stringBuilder.Append(" [-%s <float[%zu]>]", arg.name.data(), size);
                } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<double>>(floatVariant)) {
                    size_t size = std::visit([](auto arrayMember) {
                        return (t.*arrayMember).size();
                    }, *arrayMemberVariant);
                    stringBuilder.Append(" [-%s <double[%zu]>]", arg.name.data(), size);
                } else if (std::holds_alternative<float*>(*floatVariant) || std::holds_alternative<float T::*>(*floatVariant)) {
                    stringBuilder.Append(" [-%s <float>]", arg.name.data());
                } else if (std::holds_alternative<double*>(*floatVariant) || std::holds_alternative<double T::*>(*floatVariant)) {
                    stringBuilder.Append(" [-%s <double>]", arg.name.data());
                }
            } else if (auto stringVariantPtr = std::get_if<StringVariant>(&arg.argument)) {
                stringBuilder.Append(" [-%s %s]", arg.name.data(), "<string>");
            } else if (std::holds_alternative<BoolVariant>(arg.argument)) {
                stringBuilder.Append(" [-%s]", arg.name.data());
            }
        }

        auto sv = stringBuilder.GetStringView();
        printf("%.*s\n", static_cast<int>(sv.size()), sv.data());

        std::exit(0);
    }

    ParseResults ParseArgs(int argc, char** argv) {
        T t;

        int argIndex = 1;
        while (argIndex < argc) {
            std::string_view token(argv[argIndex]);
            if (token[0] != '-') {
                ReportError("Argument \"%s\" does not start with '-'\n", token.data());
                return {t, false};
            }

            if (token == "-h" ||  token == "-help" || token == "--help") {
                PrintUsageAndExit(argc, argv);
            }

            bool matchedToken = false;
            for (auto& arg : args_) {
                if (arg.name == token.substr(1)) {
                    matchedToken = true;
                    if (auto intVariant = std::get_if<IntVariant>(&arg.argument)) {
                        auto ParseInt = [argc, &argv, &argIndex, &token]() -> ParseResult<int> {
                            if (argIndex >= argc-1) {
                                ReportError("Expected int value for argument \"%s\"\n", token.data());
                                return {0, false};
                            }
                            auto valueToken = std::string_view(argv[++argIndex]);
                            int v;
                            auto result = std::from_chars(valueToken.data(), valueToken.data()+valueToken.size(), v);
                            if (result.ec == std::errc::invalid_argument) {
                                ReportError("Expected a string representing an int but instead found \"%s\"\n", valueToken.data());
                                return {v, false};
                            } else if (result.ec == std::errc::result_out_of_range) {
                                ReportError("int value \"%s\" out of range [%d, %d]\n", valueToken.data(), std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                                return {v, false};
                            }
                            return {v, true};
                        };

                        if (auto intPtr = std::get_if<int*>(intVariant)) {
                            auto [v, success] =  ParseInt();
                            if (!success) {
                                return {t, false};
                            }
                            **intPtr = v;
                        } else if (auto memberPtr = std::get_if<int T::*>(intVariant)) {
                            auto [v, success] =  ParseInt();
                            if (!success) {
                                return {t, false};
                            }
                            t.*(*memberPtr) = v;
                        } else if (auto arrayPtr = std::get_if<Array<int>>(intVariant)) {
                            if (!ParseArray(arrayPtr, ParseInt)) {
                                return {t, false};
                            }
                        } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<int>>(intVariant)) {
                            if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseInt)) {
                                return {t, false};
                            }
                        } else {
                            ReportError("Unhandled int variant\n");
                            return {t, false};
                        }
                    } else if (auto floatVariant = std::get_if<FloatVariant>(&arg.argument)) {
                        auto ParseFloat = [argc, &argv, &argIndex, &token]() -> ParseResult<float> {
                            if (argIndex >= argc-1) {
                                ReportError("Expected float value for argument \"%s\"\n", token.data());
                                return {0.0f, false};
                            }
                            auto valueToken = std::string_view(argv[++argIndex]);
                            
                            auto v = strtof(valueToken.data(), nullptr);
                            if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
                                ReportError("float value \"%s\" out of range\n", valueToken.data());
                                return {v, false};
                            }
                            return {v, true};
                        };
                        auto ParseDouble = [argc, &argv, &argIndex, &token]() -> ParseResult<double> {
                            if (argIndex >= argc-1) {
                                ReportError("Expected double value for argument \"%s\"\n", token.data());
                                return {0.0, false};
                            }
                            auto valueToken = std::string_view(argv[++argIndex]);
                            
                            auto v = strtod(valueToken.data(), nullptr);
                            if (v == HUGE_VAL || v == HUGE_VALF || v == HUGE_VALL) {
                                ReportError("double value \"%s\" out of range\n", valueToken.data());
                                return {v, false};
                            }
                            return {v, true};
                        };
                        if (auto floatPtr = std::get_if<float*>(floatVariant)) {
                            auto [v, success] = ParseFloat();
                            if (!success) {
                                return {t, false};
                            }
                            **floatPtr = v;
                        } else if (auto memberPtr = std::get_if<float T::*>(floatVariant)) {
                            auto [v, success] = ParseFloat();
                            if (!success) {
                                return {t, false};
                            }
                            t.*(*memberPtr) = v;
                        } else if (auto doublePtr = std::get_if<double*>(floatVariant)) {
                            auto [v, success] = ParseDouble();
                            if (!success) {
                                return {t, false};
                            }
                            **doublePtr = v;
                        } else if (auto memberPtr = std::get_if<double T::*>(floatVariant)) {
                            auto [v, success] = ParseDouble();
                            if (!success) {
                                return {t, false};
                            }
                            t.*(*memberPtr) = v;
                        } else if (auto arrayPtr = std::get_if<Array<float>>(floatVariant)) {
                            if (!ParseArray(arrayPtr, ParseFloat)) {
                                return {t, false};
                            }
                        } else if (auto arrayPtr = std::get_if<Array<double>>(floatVariant)) {
                            if (!ParseArray(arrayPtr, ParseDouble)) {
                                return {t, false};
                            }
                        } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<float>>(floatVariant)) {
                            if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseFloat)) {
                                return {t, false};
                            }
                        } else if (auto arrayMemberVariant = std::get_if<ArrayMemberVariant<double>>(floatVariant)) {
                            if (!ParseArrayMemberVariant(t, arrayMemberVariant, ParseDouble)) {
                                return {t, false};
                            }
                        } else {
                            ReportError("Unhandled float variant\n");
                            return {t, false};
                        }
                    } else if (auto boolVariant = std::get_if<BoolVariant>(&arg.argument)) {
                        if (auto boolPtr = std::get_if<bool*>(boolVariant)) {
                            **boolPtr = true;
                        } else if (auto memberPtr = std::get_if<bool T::*>(boolVariant)) {
                            t.*(*memberPtr) = true;
                        } else {
                            ReportError("Unhandled bool variant\n");
                            return {t, false};
                        }
                    } else if (auto stringVariant = std::get_if<StringVariant>(&arg.argument)) {
                        if (argIndex >= argc-1) {
                            ReportError("Expected string value for argument \"%s\"\n", token.data());
                            return {t, false};
                        }
                        auto valueToken = std::string_view(argv[++argIndex]);

                        if (auto stringPtr = std::get_if<std::string*>(stringVariant)) {
                            **stringPtr = valueToken;
                        } else if (auto memberPtr = std::get_if<std::string T::*>(stringVariant)) {
                            t.*(*memberPtr) = valueToken;
                        } else if (auto stringViewPtr = std::get_if<std::string_view*>(stringVariant)) {
                            **stringViewPtr = valueToken;
                        } else if (auto memberPtr = std::get_if<std::string_view T::*>(stringVariant)) {
                            t.*(*memberPtr) = valueToken;
                        } else {
                            ReportError("Unhandled string variant\n");
                            return {t, false};
                        }
                    }
                }
            }
            if (!matchedToken) {
                ReportError("Unrecognized argument \"%s\"\n", token.data());
                return {t, false};
            }
            argIndex++;
        }

        return {t, true};
    }

private:
    template <typename U>
    struct ParseResult {
        U v;
        bool success;
    };
    std::string_view description_;
    std::vector<Arg> args_;
};

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

    CommandLine<Args> cl;
    cl.Add("hello", &Args::hello);
    cl.Add("veci", &Args::veci);
    cl.Add("vecf", &Args::vecf);
    cl.Add("quat", &Args::quat);
    cl.Add("int", &Args::i);
    cl.Add("float", &Args::f);
    cl.Add("double", &Args::d);
    cl.Add("name", &Args::s);
    cl.Add("name_view", &Args::sv);
    cl.Add("raw_veci", &veci);


    cl.Add("raw_hello", &hello);
    cl.Add("raw_int", &i);
    cl.Add("raw_float", &f);
    cl.Add("raw_double", &d);
    cl.Add("raw_string", &s);
    cl.Add("raw_vecf", &vecf);
    cl.Add("raw_vecd", &vecd);
    cl.Add("raw_strview", &str_view);

    auto results = cl.ParseArgs(argc, argv);
    auto args = results.args;
    if (!results.success) {
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
