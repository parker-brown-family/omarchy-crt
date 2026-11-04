// Prints the shader the plugin would compile, then its knob names on stderr.
//   test-source crt-glass.frag > native.frag
#include "source.hpp"
#include <fstream>
#include <iostream>

int main(int, char** argv) {
    std::ifstream      f(argv[1]);
    std::vector<SKnob> knobs;
    std::cout << nativeFrom(f, knobs, 24);
    for (auto const& k : knobs)
        std::cerr << k.name << "\n";
}
