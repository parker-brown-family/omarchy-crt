// Prints the shader the plugin would compile, then its knob names on stderr.
//   test-source crt-glass.frag > native.frag
// With a knobs file, prints what the plugin would read from it instead.
//   test-source crt-glass.frag knobs.txt
// The pointer logic, which needs no compositor:
//   test-source --through x y w h px py k1 k2 clamp
//   test-source --press held dragAndDrop
#include "source.hpp"
#include <fstream>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc == 11 && std::string(argv[1]) == "--through") {
        double a[8];
        for (int i = 0; i < 8; i++)
            a[i] = std::atof(argv[i + 2]);
        const auto o = lensThrough({a[0], a[1]}, {a[2], a[3]}, {a[4], a[5]}, a[6], a[7], std::atoi(argv[10]));
        std::cout << o.x << " " << o.y << "\n";
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--press") {
        std::cout << pressHolds(std::atoi(argv[2]), std::atoi(argv[3])) << "\n";
        return 0;
    }
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
