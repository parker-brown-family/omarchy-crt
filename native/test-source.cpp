// Prints the shader the plugin would compile, then its knob names on stderr.
//   test-source crt-glass.frag > native.frag
// With a knobs file, prints what the plugin would read from it instead.
//   test-source crt-glass.frag knobs.txt
#include "source.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    std::ifstream      f(argv[1]);
    std::vector<SKnob> knobs;
    const auto         source = nativeFrom(f, knobs, 24);
    if (argc > 2) {
        std::ifstream            k(argv[2]);
        std::vector<std::string> pass;
        bool                     animated = false, on = true;
        parseKnobs(k, knobs, pass, animated, on);
        std::cout << "on=" << on << " animated=" << animated << " pass=" << pass.size();
        for (auto const& knob : knobs)
            if (knob.name == "TUBE_K1")
                std::cout << " TUBE_K1=" << knob.value[0];
        std::cout << "\n";
        return 0;
    }
    std::cout << source;
    for (auto const& k : knobs)
        std::cerr << k.name << "\n";
}
