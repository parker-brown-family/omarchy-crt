// Test rig only, never shipped. Aquamarine's headless backend has no GPU handle
// and no display to ask for buffer layouts, so on NVIDIA it cannot get a buffer
// it can render to. This interposes three of its virtual getters: the render
// node for the two handles, and for the formats whatever layout the driver
// itself picks. Left alone it labels buffers linear, which NVIDIA cannot render
// to (Aquamarine's own FIXME says so), and the framebuffer comes back incomplete.
#include <aquamarine/backend/Headless.hpp>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>

static int node() {
    static int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    return fd;
}

int Aquamarine::CHeadlessBackend::drmFD() { return node(); }
int Aquamarine::CHeadlessBackend::drmRenderNodeFD() { return node(); }

std::vector<Aquamarine::SDRMFormat> Aquamarine::CHeadlessBackend::getRenderFormats() {
    static std::vector<SDRMFormat> formats = [] {
        std::vector<SDRMFormat> out;
        auto*                   dev = gbm_create_device(node());
        for (uint32_t f : {DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_ABGR8888}) {
            auto* bo = gbm_bo_create(dev, 1600, 900, f, GBM_BO_USE_RENDERING);
            if (!bo)
                continue;
            out.push_back(SDRMFormat{.drmFormat = f, .modifiers = {gbm_bo_get_modifier(bo)}});
            gbm_bo_destroy(bo);
        }
        return out;
    }();
    return formats;
}
