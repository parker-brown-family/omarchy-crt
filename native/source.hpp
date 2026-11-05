#pragma once
// crt-glass.frag, rewritten so its knobs are uniforms and its baked tube arrays are
// uniform arrays. Kept free of Hyprland so CI can run it on the shipped shader and
// hand the result to a GLSL validator: native/test-source.cpp.
#include <algorithm>
#include <istream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

struct SKnob {
    std::string name;
    float       value[3] = {0, 0, 0};
    int         size     = 1;
    int         loc      = -1;
};

struct SPoint {
    double x = 0, y = 0;
};

// Where the shader gathers for a point in a tube: the pointer has to land there too.
// Past the tube's edge the correction stops growing and carries on at its edge value.
// It is quartic in the distance from the centre, and a press held far from a small
// window would otherwise throw the position a whole screen away.
inline SPoint lensThrough(SPoint pos, SPoint size, SPoint p, double k1, double k2, bool clamp) {
    const SPoint in = {std::clamp(p.x, pos.x, pos.x + size.x), std::clamp(p.y, pos.y, pos.y + size.y)};
    const SPoint c  = {(in.x - pos.x) / size.x - 0.5, (in.y - pos.y) / size.y - 0.5};
    const double r2 = c.x * c.x + c.y * c.y, k = 1.0 + k1 * r2 + k2 * r2 * r2;
    SPoint       w  = {0.5 + c.x * k, 0.5 + c.y * k};
    if (clamp)
        w = {std::clamp(w.x, 0.0, 0.999), std::clamp(w.y, 0.0, 0.999)};
    return {p.x + pos.x + w.x * size.x - in.x, p.y + pos.y + w.y * size.y - in.y};
}

// A press belongs to whatever it landed on until the button comes up. Without this a
// drag from a tile jumps by the whole correction as it crosses a flat toast. Drag and
// drop is the exception: the button is held, but the pointer now speaks to whatever is
// under it, so each drop target is mapped through its own tube.
inline bool pressHolds(bool held, bool dragAndDrop) {
    return held && !dragAndDrop;
}

// knobs.txt, one setting per line. A line that will not parse is skipped alone, so a
// value too large for a float cannot hide the ON or PASS lines after it.
inline void parseKnobs(std::istream& f, std::vector<SKnob>& knobs, std::vector<std::string>& pass, bool& animated, bool& on) {
    pass.clear();
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string        name;
        if (!(in >> name))
            continue;
        if (name == "ANIMATED" || name == "ON") {
            if (bool b; in >> b)
                (name == "ON" ? on : animated) = b;
        } else if (name == "PASS") {
            if (std::string cls; in >> cls)
                pass.push_back(cls);
        } else if (auto it = std::ranges::find_if(knobs, [&](auto const& k) { return k.name == name; }); it != knobs.end()) {
            float v[3] = {};
            for (int i = 0; i < it->size; i++)
                in >> v[i];
            if (in)
                std::copy(v, v + 3, it->value);
        }
    }
}

inline std::string nativeFrom(std::istream& f, std::vector<SKnob>& knobs, int maxTubes) {
    static const std::regex knob(R"(^const float (\w+)\s*=\s*(-?[0-9.]+);.*)");
    static const std::regex derived(R"(^const float (\w+)\s*=\s*([^;]+);.*)");
    static const std::regex colour(R"(^const vec3\s+(\w+)\s*=\s*vec3\(([^,]+),([^,]+),([^)]+)\);.*)");
    std::string out, line;
    std::smatch m;
    bool        inTubes = false;
    knobs.clear();
    while (std::getline(f, line)) {
        if (line.starts_with("// ---- TUBE DATA")) {
            inTubes = true;
            out += "#define TUBE_COUNT " + std::to_string(maxTubes) + "\n"
                   "uniform vec4  TUBE_RECT[TUBE_COUNT];\nuniform int   TUBE_MON[TUBE_COUNT];\n"
                   "uniform float TUBE_CURV[TUBE_COUNT];\nuniform int   TUBE_PASS[TUBE_COUNT];\n"
                   "uniform int   TUBE_QUIET[TUBE_COUNT];\n";
        } else if (line.starts_with("// ---- END TUBE DATA")) {
            inTubes = false;
        } else if (inTubes) {
            continue;
        } else if (line.starts_with("#define ANIMATED")) {
            out += "#define ANIMATED 1\n";          // the clock is a uniform too; 0 stands still
        } else if (std::regex_match(line, m, knob)) {
            knobs.push_back({.name = m[1], .value = {std::stof(m[2])}, .size = 1});
            out += "uniform float " + m[1].str() + ";\n";
        } else if (std::regex_match(line, m, derived)) {
            out += "#define " + m[1].str() + " (" + m[2].str() + ")\n";
        } else if (std::regex_match(line, m, colour)) {
            knobs.push_back({.name = m[1], .value = {std::stof(m[2]), std::stof(m[3]), std::stof(m[4])}, .size = 3});
            out += "uniform vec3 " + m[1].str() + ";\n";
        } else {
            out += line + "\n";
        }
    }
    return out;
}
