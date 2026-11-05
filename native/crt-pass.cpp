// CRT pass: the optional native path, loaded by `crt native on`.
//
// The screen shader runs after everything has been composited, so a toast is
// already part of the picture it bends. This runs the same tube warp after the
// windows and BEFORE top and overlay layers, so those land flat on a curved
// tile, and Hyprland draws them as usual. Tile rects are uniforms read from the
// compositor every frame, so a layout change never compiles anything.
#include <GLES3/gl32.h>
#include <algorithm>
#include <dlfcn.h>
#include <filesystem>
#include <regex>
#include <string>
#include <map>
#include <cmath>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <cstdlib>
#include "source.hpp"
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/desktop/history/WindowHistoryTracker.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRuleApplicator.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#define private public     // m_currentlyHeldButtons, read-only here
#include <hyprland/src/managers/input/InputManager.hpp>
#undef private
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/desktop/state/ViewStateTracker.hpp>
#include <hyprland/src/desktop/state/ViewHitTester.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/protocols/wlr-layer-shell-unstable-v1.hpp>
#include <hyprland/src/protocols/core/DataDevice.hpp>

static HANDLE PHANDLE = nullptr;

// The test seat's switches. A real build has none of this, and does not stat files
// every frame.
#ifdef CRT_RIG
static bool rig(const char* name) {
    return access((std::string(getenv("HOME")) + "/" + name).c_str(), F_OK) == 0;
}
#else
static constexpr bool rig(const char*) { return false; }
#endif
static long   g_frames = 0, g_programs = 0;

static const char* VS = R"(#version 320 es
out vec2 v_texcoord;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_texcoord = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";

// THE LOOK LIVES IN ONE PLACE. crt-glass.frag is compiled here as it ships, with
// its knobs rewritten from constants into uniforms and its baked tube arrays into
// uniform arrays. So there is no second copy of the optics to drift, and a knob or
// a layout change is a uniform write, never a compile.
static constexpr int MAX_TUBES = 24;

static std::vector<SKnob>       g_knobs;
static std::vector<std::string> g_passthrough;     // window classes that keep their own picture
static bool                     g_animated = false;
static bool                     g_on       = true;

static std::string pluginDir() {
    Dl_info i{};
    dladdr((void*)&pluginDir, &i);
    std::string p = i.dli_fname ? i.dli_fname : "";
    return p.substr(0, p.rfind('/'));
}

static std::string nativeSource() {
    std::ifstream f(pluginDir() + "/crt-glass.frag");
    return f ? nativeFrom(f, g_knobs, MAX_TUBES) : std::string{};
}

static float knob(const char* name, float fallback) {
    for (auto const& k : g_knobs)
        if (k.name == name)
            return k.value[0];
    return fallback;
}

// knobs.txt is what `crt` publishes: "NAME value", or "NAME r g b" for a colour.
static std::filesystem::file_time_type g_knobsSeen{};
static void readKnobs() {
    std::error_code ec;
    const auto      path = pluginDir() + "/knobs.txt";
    const auto      t    = std::filesystem::last_write_time(path, ec);
    if (ec || t == g_knobsSeen)
        return;
    g_knobsSeen = t;
    std::ifstream f(path);
    parseKnobs(f, g_knobs, g_passthrough, g_animated, g_on);
}

static std::string g_source;

static GLuint build() {
    auto sh = [](GLenum t, const std::string& s) {
        GLuint      o = glCreateShader(t);
        const char* c = s.c_str();
        glShaderSource(o, 1, &c, nullptr);
        glCompileShader(o);
        GLint ok = 0;
        glGetShaderiv(o, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char l[2048] = {};
            glGetShaderInfoLog(o, sizeof l, nullptr, l);
            std::ofstream(pluginDir() + "/crt-pass.err", std::ios::app) << l << "\n";
        }
        return o;
    };
    GLuint p = glCreateProgram();
    glAttachShader(p, sh(GL_VERTEX_SHADER, VS));
    glAttachShader(p, sh(GL_FRAGMENT_SHADER, g_source));
    glLinkProgram(p);
    g_programs++;
    return p;
}

// One scratch copy per framebuffer size, so two monitors do not reallocate every
// frame. The program and the empty vertex array are shared.
struct SScratch {
    GLuint tex = 0, fb = 0;
};
static std::map<std::pair<int, int>, SScratch> g_scratch;
static GLuint                                  g_prog = 0, g_vao = 0;
static bool                                    g_located = false;

// One surface's tube. Drawing and hit-testing both read this list, in this order,
// so a pixel and a click can never disagree about which window owns them.
struct STube {
    CBox  fb;        // in the framebuffer, for the shader
    CBox  logical;   // in the layout, for the pointer
    float curv  = 1;
    int   pass  = 0;
    int   quiet = 0;
};

class CCrtPass : public IPassElement {
  public:
    explicit CCrtPass(std::vector<STube> tubes) : m_tubes(std::move(tubes)) {}
    bool        needsLiveBlur() override { return false; }
    bool        needsPrecomputeBlur() override { return false; }
    const char* passName() override { return "CCrtPass"; }
    ePassElementType type() override { return EK_CUSTOM; }
    bool        undiscardable() override { return true; }
    // The gather reads away from each output pixel, so nothing beneath may be culled.
    bool        disableSimplification() override { return true; }

    std::vector<UP<IPassElement>> draw() override {
        g_frames++;
#ifdef CRT_RIG
        std::ofstream(std::string(getenv("HOME")) + "/frames.now") << g_frames << "\n";
        const bool timed = rig("time-pass");
        if (timed) glFinish();
        const auto t0 = std::chrono::steady_clock::now();
#endif
        GLint target = 0, vp[4] = {}, prog = 0, vao = 0, active = 0, tex = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
        glGetIntegerv(GL_VIEWPORT, vp);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        const GLboolean blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST);
        const int w = vp[2], h = vp[3];

        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
        if (!g_prog)
            g_prog = build();
        if (!g_vao)
            glGenVertexArrays(1, &g_vao);
        if (!g_located) {
            for (auto& k : g_knobs)
                k.loc = glGetUniformLocation(g_prog, k.name.c_str());
            g_located = true;
        }
        auto& sc = g_scratch[{w, h}];
        if (!sc.tex) {
            glGenTextures(1, &sc.tex);
            glBindTexture(GL_TEXTURE_2D, sc.tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &sc.fb);
            glBindFramebuffer(GL_FRAMEBUFFER, sc.fb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sc.tex, 0);
        }

        // Never sample the attachment being drawn to: the lower scene is copied first.
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sc.fb);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, target);

        glDisable(GL_BLEND);
        glUseProgram(g_prog);
        glBindVertexArray(g_vao);
        glBindTexture(GL_TEXTURE_2D, sc.tex);
        for (auto const& k : g_knobs) {
            if (k.size == 3) glUniform3fv(k.loc, 1, k.value);
            else glUniform1f(k.loc, k.value[0]);
        }
        static const auto start = std::chrono::steady_clock::now();
        const float       now   = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        glUniform1i(glGetUniformLocation(g_prog, "tex"), 0);
        glUniform1f(glGetUniformLocation(g_prog, "time"), g_animated ? now : 0.F);
        glUniform1i(glGetUniformLocation(g_prog, "wl_output"), 0);
        // TUBE_MON is -1 past the live rects, which tube_at skips as another monitor.
        std::array<int, MAX_TUBES>   mon, pass{}, quiet{};
        std::array<float, MAX_TUBES> curv{};
        std::array<float, MAX_TUBES * 4> rect{};
        mon.fill(-1);
        for (size_t i = 0; i < m_tubes.size() && i < MAX_TUBES; i++) {
            auto const& t = m_tubes[i];
            mon[i] = 0; curv[i] = t.curv; pass[i] = t.pass; quiet[i] = t.quiet;
            rect[i * 4] = t.fb.x; rect[i * 4 + 1] = t.fb.y; rect[i * 4 + 2] = t.fb.w; rect[i * 4 + 3] = t.fb.h;
        }
        glUniform1iv(glGetUniformLocation(g_prog, "TUBE_MON"), MAX_TUBES, mon.data());
        glUniform1fv(glGetUniformLocation(g_prog, "TUBE_CURV"), MAX_TUBES, curv.data());
        glUniform1iv(glGetUniformLocation(g_prog, "TUBE_PASS"), MAX_TUBES, pass.data());
        glUniform1iv(glGetUniformLocation(g_prog, "TUBE_QUIET"), MAX_TUBES, quiet.data());
        glUniform4fv(glGetUniformLocation(g_prog, "TUBE_RECT"), MAX_TUBES, rect.data());
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindTexture(GL_TEXTURE_2D, tex);
        glActiveTexture(active);
        glBindVertexArray(vao);
        glUseProgram(prog);
        if (blend) glEnable(GL_BLEND);
        if (scissor) glEnable(GL_SCISSOR_TEST);
#ifdef CRT_RIG
        if (timed) {
            glFinish();
            static double total = 0;
            static long   n     = 0;
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::ofstream(std::string(getenv("HOME")) + "/pass-ms.txt") << total / ++n << " " << n << " " << w << "x" << h << "\n";
        }
#endif
        return {};
    }

  private:
    std::vector<STube> m_tubes;
};

// Test rig: the finished frame, read back after every layer has been drawn.
class CDump : public IPassElement {
  public:
    bool        needsLiveBlur() override { return false; }
    bool        needsPrecomputeBlur() override { return false; }
    const char* passName() override { return "CDump"; }
    ePassElementType type() override { return EK_CUSTOM; }
    bool        undiscardable() override { return true; }
    bool        disableSimplification() override { return true; }
    std::vector<UP<IPassElement>> draw() override {
        const std::string home = getenv("HOME");
        std::ifstream     want(home + "/dump-now");
        if (!want) return {};
        std::string name;
        want >> name;     // empty means any monitor
        auto m = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (!name.empty() && (!m || m->m_name != name)) return {};
        GLint vp[4] = {}, target = 0;
        glGetIntegerv(GL_VIEWPORT, vp);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target);
        std::vector<unsigned char> px((size_t)vp[2] * vp[3] * 4);
        glReadPixels(0, 0, vp[2], vp[3], GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        std::ofstream(home + "/frame.rgba", std::ios::binary).write((char*)px.data(), px.size());
        std::ofstream(home + "/frame.txt") << vp[2] << " " << vp[3] << " frames=" << g_frames << " programs=" << g_programs << "\n";
        unlink((home + "/dump-now").c_str());
        return {};
    }
};

// The geometry's own rule, read live: floating windows above tiles, focus order
// within each group, a float's border inside its frame, and Terminal Delight flat
// because it curves its own panes.
static std::vector<STube> tubes(PHLMONITOR m) {
    struct SRow {
        PHLWINDOW w;
        int       group, focus;
    };
    const auto&       history = Desktop::History::windowTracker()->fullHistory();
    std::vector<SRow> rows;
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isMapped || w->isHidden() || !g_pHyprRenderer->shouldRenderWindow(w, m))
            continue;
        // A window behind a fullscreen one is still mapped, and still listed. It
        // must not claim pixels it is not drawing.
        if (w->alphaValue(Desktop::View::WINDOW_ALPHA_FADE) * w->alphaValue(Desktop::View::WINDOW_ALPHA_FULLSCREEN) == 0.F)
            continue;
        int focus = -1;
        for (size_t i = 0; i < history.size(); i++)
            if (history[i].lock() == w)
                focus = history.size() - i - 1;
        rows.push_back({w, w->m_isFloating ? 0 : 1, focus});
    }
    std::ranges::stable_sort(rows, [](auto const& a, auto const& b) { return std::tie(a.group, a.focus) < std::tie(b.group, b.focus); });

    std::vector<STube> out;
    for (auto const& r : rows) {
        const auto& w = r.w;
        if (out.size() >= MAX_TUBES)
            break;
        CBox logical = {w->positionAnimation()->value(), w->sizeAnimation()->value()};
        // Where the renderer really draws it: a sliding workspace carries its windows.
        if (w->m_workspace && !w->m_pinned && w->m_workspace->m_renderOffset->isBeingAnimated())
            logical.translate(w->m_workspace->m_renderOffset->value());
        logical.translate(w->m_floatingOffset);
        // The border goes inside the glass. Left outside, the tube's black corner eats
        // the rounded arc and leaves a square block with stubs of border around it.
        if (!Fullscreen::controller()->isFullscreen(w) && w->m_ruleApplicator->decorate().valueOrDefault())
            logical.expand(w->getRealBorderSize());
        // The framebuffer is in the panel's own orientation, not the rotated one a
        // person sees. Only the rect turns: the warp is symmetric in x and y.
        CBox fb = {(logical.pos() - m->m_position) * m->m_scale, logical.size() * m->m_scale};
        fb.transform(Math::wlTransformToHyprutils(Math::invertTransform(m->m_transform)), m->m_transformedSize.x, m->m_transformedSize.y);
        const auto cls = w->m_class;
        out.push_back({.fb = fb, .logical = logical, .curv = cls == "terminal-delight" ? 0.F : 1.F,
                       .pass = std::ranges::find(g_passthrough, cls) != g_passthrough.end(),
                       .quiet = m != Desktop::focusState()->monitor()});
    }
    return out;
}

// THE POINTER. The picture now bends and the pointer does not, so a pixel on
// screen shows the source the shader gathers for it, and a click must land there.
// The move routine is the one place hit-testing happens, so only while it runs
// does "where is the pointer" answer with the mapped position. The drawn cursor and
// keybinds keep the true one. Flat layers are hit where they are drawn. Drag and drop
// motion comes out of the move routine too, so a drop target gets the mapped position.
// (overridePos looks like the tool for this and is not: it skips sending motion.)
static CFunctionHook*          g_positionHook = nullptr;
static CFunctionHook*          g_moveHook     = nullptr;
static std::optional<Vector2D> g_mapped;

static CFunctionHook*          g_layerHook    = nullptr;

typedef Vector2D (*origPosition)(void*);
typedef SP<CWLSurfaceResource> (*origLayerAt)(void*, const Vector2D&, std::vector<PHLLSREF>*, Vector2D*, PHLLS*, bool);
typedef void (*origMove)(void*, uint32_t, bool, bool, std::optional<Vector2D>);

static Vector2D hkPosition(void* self) {
    return g_mapped ? *g_mapped : ((origPosition)g_positionHook->m_original)(self);
}

// A mapped move has already found no layer under the true position. Asked again
// with the mapped one, a pointer in the gap between two cards would hit a card.
static SP<CWLSurfaceResource> hkLayerAt(void* self, const Vector2D& pos, std::vector<PHLLSREF>* layers, Vector2D* coords, PHLLS* found, bool lock) {
    if (g_mapped)
        return nullptr;
    return ((origLayerAt)g_layerHook->m_original)(self, pos, layers, coords, found, lock);
}

// Past a tube's edge the correction stops growing and carries on at its edge value. It
// is quartic in the distance from the centre, and a press held far from a small window
// would otherwise throw the position a whole screen away.
static Vector2D through(const CBox& box, const Vector2D& p, bool clamp) {
    const Vector2D in = {std::clamp(p.x, box.x, box.x + box.w), std::clamp(p.y, box.y, box.y + box.h)};
    const auto   c  = (in - box.pos()) / box.size() - Vector2D{0.5, 0.5};
    const double r2 = c.x * c.x + c.y * c.y, k = 1.0 + knob("TUBE_K1", 0) * r2 + knob("TUBE_K2", 0) * r2 * r2;
    Vector2D     w  = Vector2D{0.5, 0.5} + c * k;
    if (clamp)
        w = {std::clamp(w.x, 0.0, 0.999), std::clamp(w.y, 0.0, 0.999)};
    return p + box.pos() + w * box.size() - in;
}

// A press belongs to whatever it landed on until the button comes up. Without this a
// drag from a tile jumps by the whole correction as it crosses a flat toast. Drag and
// drop is the exception: the button is held, but the pointer now speaks to whatever is
// under it, so each drop target is mapped through its own tube.
static std::optional<CBox> g_pressTube;

static std::optional<Vector2D> mapped(const Vector2D& p) {
    // The lock screen is flat and its surfaces are hit where they are drawn. The
    // windows behind it are still mapped, so this must be asked before anything else.
    // A pointer a game has locked or confined is that game's: its motion is relative,
    // and correcting the position would fight the confinement.
    if (rig("input-flat") || !g_on || g_pSessionLockManager->isSessionLocked() || g_pInputManager->isConstrained()) {
        g_pressTube.reset();
        return std::nullopt;
    }
    if (!g_pInputManager->m_currentlyHeldButtons.empty() && !PROTO::data->dndActive())
        return g_pressTube ? std::optional{through(*g_pressTube, p, false)} : std::nullopt;
    g_pressTube.reset();
    for (auto const& m : State::monitorState()->monitors()) {
        if (!m->logicalBox().containsPoint(p))
            continue;
        Vector2D coords;
        PHLLS    layer;
        for (auto const lvl : {ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, ZWLR_LAYER_SHELL_V1_LAYER_TOP})
            if (Desktop::viewState()->hitTest().layerSurfaceAt(p, &m->m_layerSurfaceLayers[lvl], &coords, &layer))
                return std::nullopt;
        for (auto const& t : tubes(m)) {
            if (!t.logical.containsPoint(p))
                continue;
            if (t.curv == 0 || t.pass)
                return std::nullopt;
            g_pressTube = t.logical;
            return through(t.logical, p, true);
        }
    }
    return std::nullopt;
}

static void hkMove(void* self, uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> override_) {
    if (!override_)
        g_mapped = mapped(((origPosition)g_positionHook->m_original)(Pointer::mgr().get()));
    ((origMove)g_moveHook->m_original)(self, time, refocus, mouse, override_);
    g_mapped.reset();
}

static CFunctionHook* hookNamed(const std::string& name, const std::string& demangledHas, void* to) {
    for (auto const& f : HyprlandAPI::findFunctionsByName(PHANDLE, name))
        if (f.demangled.contains(demangledHas)) {
            auto* h = HyprlandAPI::createFunctionHook(PHANDLE, f.address, to);
            if (h && h->hook())
                return h;
        }
    throw std::runtime_error("crt-pass: cannot hook " + name);
}

static CHyprSignalListener g_stage;

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;
    // The hooks below are tied to this exact Hyprland. Refuse anything else, and
    // `crt native on` falls back to the screen shader.
#ifdef CRT_FORCE_MISMATCH
    throw std::runtime_error("crt-pass: built for another Hyprland (forced)");
#endif
    if (std::string(__hyprland_api_get_hash()) != std::string(__hyprland_api_get_client_hash()))
        throw std::runtime_error("crt-pass: built for another Hyprland, rebuild with native/build");
    g_positionHook = hookNamed("position", "CPointerManager::position", (void*)hkPosition);
    g_layerHook    = hookNamed("layerSurfaceAt", "CViewHitTester::layerSurfaceAt", (void*)hkLayerAt);
    g_moveHook     = hookNamed("mouseMoveUnified", "CInputManager::mouseMoveUnified", (void*)hkMove);
    // Before anything draws, so the knob list exists when knobs.txt is first read.
    // Hyprland keeps an unloaded plugin mapped, so a second load of one build finds these
    // statics as they were left: the program and its scratch are still good, and the knob
    // list has just been rebuilt with no locations and no values.
    const auto linked = g_source;
    g_source = nativeSource();
    if (g_source.empty())
        throw std::runtime_error("crt-pass: no crt-glass.frag beside the plugin");
    if (g_source != linked)
        g_prog = 0;
    g_located   = false;
    g_knobsSeen = {};
    readKnobs();
    g_stage = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
#ifdef CRT_RIG
        static long seen[8] = {};
        seen[stage & 7]++;
        if (seen[stage & 7] % 30 == 1) {
            std::ofstream f(std::string(getenv("HOME")) + "/stages.txt");
            for (long n : seen) f << n << " ";
            f << "drawn=" << g_frames << "\n";
        }
#endif
        // Test rig: a still screen rightly draws nothing, and a capture needs a
        // frame, so a waiting request keeps the seat drawing until it is served.
        // The clock wants the same. Damaged here, not through doLater, because a
        // queued lambda would outlive an unload.
        if (stage == RENDER_POST && ((g_animated && g_on) || rig("keep-drawing")))
            for (auto& mon : State::monitorState()->monitors())
                g_pHyprRenderer->damageMonitor(mon);
        // A lone fullscreen window takes a shortcut that never reaches the stage this
        // pass runs at, so the shortcut is taken away before it is looked at.
        if (stage == RENDER_PRE)
            for (auto& mon : State::monitorState()->monitors())
                mon->m_solitaryClient.reset();
        auto m = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (!m) return;
        // Test rig: this file moves the pass to after the layers, which is where
        // the screen shader sits, so one scene can show today beside the fix.
        // Read here, not in the pass, because a stood-down pass never runs to read it.
        if (stage == RENDER_POST_WINDOWS)
            readKnobs();
        // Test rig: pass-off stands the pass down so today's screen shader can run alone.
        if (!g_on || rig("pass-off")) {
        } else if (stage == (rig("pass-late") ? RENDER_LAST_MOMENT : RENDER_POST_WINDOWS))
            g_pHyprRenderer->m_renderPass.add(makeUnique<CCrtPass>(tubes(m)));
#ifdef CRT_RIG
        if (stage == RENDER_LAST_MOMENT)
            g_pHyprRenderer->m_renderPass.add(makeUnique<CDump>());
#endif
    });
#ifndef CRT_VERSION
#define CRT_VERSION "0.1.0"
#endif
    return {"crt-pass", "The CRT, below top and overlay layers", "Parker Brown", CRT_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() { 
    g_stage.reset();
    g_mapped.reset();
    // Hyprland keeps a frame's pass elements until the next frame begins, and would
    // then destroy ours with code that has been unloaded.
    g_pHyprRenderer->m_renderPass.clear();
}
