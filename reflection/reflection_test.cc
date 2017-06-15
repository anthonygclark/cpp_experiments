
//////
// Typically this block would be in a cpp file called ReflectionTestImpl.cc
// or something. It's inline here for demo reasons.

#include <string>
#include <vector>
#include <iostream>

#define NAMESPACE_NAME refl_objs
#define OBJECT_NAME ReflectionTest
#define DEFINITION_FILE reflection_test.incl
#include "reflection.hh"
#undef NAMESPACE_NAME
#undef OBJECT_NAME
#undef DEFINITION_FILE

//////

#include <regex>
#include <iostream>

int main()
{
    refl_objs::ReflectionTest r{};

    // print defaults
    std::string addr; 
    std::cout << "Address v1: " << r.get_address() << std::endl;
    r.get(refl_objs::ReflectionTest::ADDRESS, addr);
    std::cout << "Address v2: " << addr << std::endl;
    r.set("address", "127.0.0.1");
    r.get("address", addr);
    std::cout << "Address v3: " << addr << std::endl;

    std::cout << "latitude: " << r.get_latitude() << std::endl;

    // Set via property sette via property setter
    r.set_vecta({1,2,3});
    for (auto & i : r.get_vecta())
        std::cout << "VECTA: " << i << std::endl;

    // Parse something from string
    std::string input{"address=test,latitude=123.123f"};
    std::regex sep{"=|,"};
    std::sregex_token_iterator end_of_seq;
    std::sregex_token_iterator token{input.begin(), input.end(), sep, -1};

    while (token != end_of_seq)
    {
        std::string k = *token++;
        std::string v = *token++;

        if (k == "latitude") {
            std::cout << "Setting '" << k << "' to '" << v << "' via regex\n";
            r.set(k, std::stof(v));
        }
        else if (k == "address") {
            std::cout << "Setting '" << k << "' to '" << v << "' via regex\n";
            r.set(k, v);
        }
    }

    std::cout << "Final address: " << r.get_address() << std::endl
              << "Final latitude: " << std::fixed << r.get_latitude() << std::endl;
}
