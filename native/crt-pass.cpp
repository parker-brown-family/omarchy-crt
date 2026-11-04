// CRT pass: proof of concept, runs only in the sealed test seat (native/rig).
//
// The screen shader runs after everything has been composited, so a toast is
// already part of the picture it bends. This runs the same tube warp after the
// windows and BEFORE top and overlay layers, so those land flat on a curved
// tile, and Hyprland draws them as usual. Tile rects are uniforms read from the
// compositor every frame, so a layout change never compiles anything.
#include <GLES3/gl32.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <cstdlib>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/desktop/state/ViewStateTracker.hpp>
#include <hyprland/src/desktop/state/ViewHitTester.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/protocols/wlr-layer-shell-unstable-v1.hpp>

static HANDLE PHANDLE = nullptr;
static constexpr double K1 = 0.26, K2 = 0.11;    // the shader's, and they must agree
static long   g_frames = 0, g_programs = 0;

static const char* VS = R"(#version 300 es
out vec2 uv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";

// tube_map is crt-glass.frag's, with rects as uniforms instead of baked arrays.
static const char* FS = R"(#version 300 es
precision highp float;
#define MAX_TUBES 24
in vec2 uv;
out vec4 frag;
uniform sampler2D tex;
uniform vec2 res;
uniform int count;
uniform vec4 rects[MAX_TUBES];
const float K1 = 0.26, K2 = 0.11, SCAN = 0.16;
void main() {
    vec2 px = uv * res;   // Hyprland's framebuffers are top-down, as rects are
    for (int i = 0; i < MAX_TUBES; i++) {
        if (i >= count) break;
        vec4 r = rects[i];
        vec2 l = (px - r.xy) / r.zw;
        if (l.x < 0.0 || l.x > 1.0 || l.y < 0.0 || l.y > 1.0) continue;
        vec2  c  = l - 0.5;
        float r2 = dot(c, c);
        vec2  w  = 0.5 + c * (1.0 + K1 * r2 + K2 * r2 * r2);
        vec2  e  = min(w, 1.0 - w);
        float edge = smoothstep(0.0, 0.004, min(e.x, e.y));
        if (edge <= 0.0) { frag = vec4(0.0, 0.0, 0.0, 1.0); return; }
        vec2  s  = (r.xy + clamp(w, 0.0, 1.0) * r.zw) / res;
        vec3  col = texture(tex, s).rgb;
        col *= 1.0 - SCAN * step(1.0, mod(gl_FragCoord.y, 3.0)) * 0.5;
        col *= 1.0 - dot(c, c) * 0.5;
        frag = vec4(col * edge, 1.0);
        return;
    }
    frag = vec4(texture(tex, uv).rgb, 1.0);
})";

static GLuint build() {
    auto sh = [](GLenum t, const char* s) {
        GLuint o = glCreateShader(t);
        glShaderSource(o, 1, &s, nullptr);
        glCompileShader(o);
        GLint ok = 0;
        glGetShaderiv(o, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char l[1024] = {};
            glGetShaderInfoLog(o, sizeof l, nullptr, l);
            std::ofstream(std::string(getenv("HOME")) + "/crt-pass.err", std::ios::app) << l << "\n";
        }
        return o;
    };
    GLuint p = glCreateProgram();
    glAttachShader(p, sh(GL_VERTEX_SHADER, VS));
    glAttachShader(p, sh(GL_FRAGMENT_SHADER, FS));
    glLinkProgram(p);
    g_programs++;
    return p;
}

struct SScratch {
    GLuint tex = 0, fb = 0, prog = 0, vao = 0;
    int    w = 0, h = 0;
} static g;

class CCrtPass : public IPassElement {
  public:
    explicit CCrtPass(std::vector<std::array<float, 4>> rects) : m_rects(std::move(rects)) {}
    bool        needsLiveBlur() override { return false; }
    bool        needsPrecomputeBlur() override { return false; }
    const char* passName() override { return "CCrtPass"; }
    ePassElementType type() override { return EK_CUSTOM; }
    bool        undiscardable() override { return true; }
    // The gather reads away from each output pixel, so nothing beneath may be culled.
    bool        disableSimplification() override { return true; }

    std::vector<UP<IPassElement>> draw() override {
        g_frames++;
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
        if (!g.prog) {
            g.prog = build();
            glGenVertexArrays(1, &g.vao);
        }
        if (g.w != w || g.h != h) {
            if (g.tex) { glDeleteTextures(1, &g.tex); glDeleteFramebuffers(1, &g.fb); }
            glGenTextures(1, &g.tex);
            glBindTexture(GL_TEXTURE_2D, g.tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &g.fb);
            glBindFramebuffer(GL_FRAMEBUFFER, g.fb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tex, 0);
            g.w = w; g.h = h;
        }

        // Never sample the attachment being drawn to: the lower scene is copied first.
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g.fb);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, target);

        glDisable(GL_BLEND);
        glUseProgram(g.prog);
        glBindVertexArray(g.vao);
        glBindTexture(GL_TEXTURE_2D, g.tex);
        glUniform1i(glGetUniformLocation(g.prog, "tex"), 0);
        glUniform2f(glGetUniformLocation(g.prog, "res"), w, h);
        glUniform1i(glGetUniformLocation(g.prog, "count"), (int)m_rects.size());
        if (!m_rects.empty())
            glUniform4fv(glGetUniformLocation(g.prog, "rects"), (int)m_rects.size(), m_rects[0].data());
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindTexture(GL_TEXTURE_2D, tex);
        glActiveTexture(active);
        glBindVertexArray(vao);
        glUseProgram(prog);
        if (blend) glEnable(GL_BLEND);
        if (scissor) glEnable(GL_SCISSOR_TEST);
        return {};
    }

  private:
    std::vector<std::array<float, 4>> m_rects;
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
        if (access((home + "/dump-now").c_str(), F_OK) != 0) return {};
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

static std::vector<std::array<float, 4>> tubes(PHLMONITOR m) {
    std::vector<std::array<float, 4>> out;
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isMapped || w->isHidden() || w->m_monitor.lock() != m || out.size() >= 24)
            continue;
        const auto pos = (w->positionAnimation()->value() - m->m_position) * m->m_scale;
        const auto sz  = w->sizeAnimation()->value() * m->m_scale;
        out.push_back(std::array<float, 4>{(float)pos.x, (float)pos.y, (float)sz.x, (float)sz.y});
    }
    return out;
}

// THE POINTER. The picture now bends and the pointer does not, so a pixel on
// screen shows the source the shader gathers for it, and a click must land there.
// The move routine is the one place hit-testing happens, so only while it runs
// does "where is the pointer" answer with the mapped position. The drawn cursor,
// drags and keybinds keep the true one. Flat layers are hit where they are drawn.
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

static std::optional<Vector2D> mapped(const Vector2D& p) {
    if (access((std::string(getenv("HOME")) + "/input-flat").c_str(), F_OK) == 0)   // test rig: today's miss
        return std::nullopt;
    for (auto const& m : State::monitorState()->monitors()) {
        if (!m->logicalBox().containsPoint(p))
            continue;
        Vector2D coords;
        PHLLS    layer;
        for (auto const lvl : {ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, ZWLR_LAYER_SHELL_V1_LAYER_TOP})
            if (Desktop::viewState()->hitTest().layerSurfaceAt(p, &m->m_layerSurfaceLayers[lvl], &coords, &layer))
                return std::nullopt;
        for (auto const& w : Desktop::windowState()->windows()) {
            if (!w->m_isMapped || w->isHidden() || w->m_monitor.lock() != m)
                continue;
            const auto pos = w->positionAnimation()->value(), size = w->sizeAnimation()->value();
            const auto c = (p - pos) / size - Vector2D{0.5, 0.5};
            if (std::abs(c.x) > 0.5 || std::abs(c.y) > 0.5)
                continue;
            const double r2 = c.x * c.x + c.y * c.y, k = 1.0 + K1 * r2 + K2 * r2 * r2;
            const double x = std::clamp(0.5 + c.x * k, 0.0, 0.999), y = std::clamp(0.5 + c.y * k, 0.0, 0.999);
            return pos + Vector2D{x, y} * size;
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
    g_positionHook = hookNamed("position", "CPointerManager::position", (void*)hkPosition);
    g_layerHook    = hookNamed("layerSurfaceAt", "CViewHitTester::layerSurfaceAt", (void*)hkLayerAt);
    g_moveHook     = hookNamed("mouseMoveUnified", "CInputManager::mouseMoveUnified", (void*)hkMove);
    g_stage = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
        static long seen[8] = {};
        seen[stage & 7]++;
        if (seen[stage & 7] % 30 == 1) {
            std::ofstream f(std::string(getenv("HOME")) + "/stages.txt");
            for (long n : seen) f << n << " ";
            f << "drawn=" << g_frames << "\n";
        }
        // Test rig: a still screen rightly draws nothing, and a capture needs a
        // frame, so a waiting request keeps the seat drawing until it is served.
        if (stage == RENDER_POST && access((std::string(getenv("HOME")) + "/keep-drawing").c_str(), F_OK) == 0)
            g_pEventLoopManager->doLater([] {
                for (auto& mon : State::monitorState()->monitors())
                    g_pHyprRenderer->damageMonitor(mon);
            });
        auto m = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (!m) return;
        // Test rig: this file moves the pass to after the layers, which is where
        // the screen shader sits, so one scene can show today beside the fix.
        const bool late = access((std::string(getenv("HOME")) + "/pass-late").c_str(), F_OK) == 0;
        if (stage == (late ? RENDER_LAST_MOMENT : RENDER_POST_WINDOWS))
            g_pHyprRenderer->m_renderPass.add(makeUnique<CCrtPass>(tubes(m)));
        if (stage == RENDER_LAST_MOMENT)
            g_pHyprRenderer->m_renderPass.add(makeUnique<CDump>());
    });
    return {"crt-pass", "CRT below top and overlay layers (proof of concept)", "Parker Brown", "0.0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() { g_stage.reset(); g_mapped.reset(); }
