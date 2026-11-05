// Test rig: a game's pointer, in miniature. Locks the pointer on the first click and
// logs what arrives while locked, then lets go after enough motion. It reports to
// stdout: LOCKED, REL dx dy, UNLOCKED.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include "pointer-constraints-unstable-v1.h"
#include "relative-pointer-unstable-v1.h"
#include "xdg-shell.h"

static struct wl_compositor*                    comp;
static struct wl_shm*                           shm;
static struct wl_seat*                          seat;
static struct xdg_wm_base*                      wm;
static struct zwp_pointer_constraints_v1*       cons;
static struct zwp_relative_pointer_manager_v1*  relmgr;
static struct wl_surface*                       surf;
static struct wl_pointer*                       ptr;
static struct zwp_locked_pointer_v1*            lock;
static int                                      seen, W = 500, H = 300;

static void on_global(void* d, struct wl_registry* r, uint32_t id, const char* n, uint32_t v) {
    if (!strcmp(n, "wl_compositor")) comp = wl_registry_bind(r, id, &wl_compositor_interface, 4);
    if (!strcmp(n, "wl_shm")) shm = wl_registry_bind(r, id, &wl_shm_interface, 1);
    if (!strcmp(n, "wl_seat")) seat = wl_registry_bind(r, id, &wl_seat_interface, 5);
    if (!strcmp(n, "xdg_wm_base")) wm = wl_registry_bind(r, id, &xdg_wm_base_interface, 1);
    if (!strcmp(n, "zwp_pointer_constraints_v1")) cons = wl_registry_bind(r, id, &zwp_pointer_constraints_v1_interface, 1);
    if (!strcmp(n, "zwp_relative_pointer_manager_v1")) relmgr = wl_registry_bind(r, id, &zwp_relative_pointer_manager_v1_interface, 1);
}
static void on_remove(void* d, struct wl_registry* r, uint32_t id) {}
static const struct wl_registry_listener reg = {on_global, on_remove};
static void wm_ping(void* d, struct xdg_wm_base* b, uint32_t s) { xdg_wm_base_pong(b, s); }
static const struct xdg_wm_base_listener wml = {wm_ping};

static void draw(void) {
    int   stride = W * 4, size = stride * H, fd = memfd_create("locker", 0);
    ftruncate(fd, size);
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(p, 0x40, size);
    struct wl_shm_pool* pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer*   b    = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_XRGB8888);
    wl_surface_attach(surf, b, 0, 0);
    wl_surface_damage(surf, 0, 0, W, H);
    wl_surface_commit(surf);
}
static void cfg(void* d, struct xdg_surface* s, uint32_t serial) {
    xdg_surface_ack_configure(s, serial);
    draw();
}
static const struct xdg_surface_listener xsl = {cfg};
static void top_cfg(void* d, struct xdg_toplevel* t, int32_t w, int32_t h, struct wl_array* a) {
    if (w > 0 && h > 0) W = w, H = h;
}
static void top_close(void* d, struct xdg_toplevel* t) { exit(0); }
static const struct xdg_toplevel_listener tl = {top_cfg, top_close};

static void locked(void* d, struct zwp_locked_pointer_v1* l) { puts("LOCKED"); fflush(stdout); }
static void unlocked(void* d, struct zwp_locked_pointer_v1* l) { puts("UNLOCKED"); fflush(stdout); }
static const struct zwp_locked_pointer_v1_listener ll = {locked, unlocked};

static void rel(void* d, struct zwp_relative_pointer_v1* r, uint32_t a, uint32_t b, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t ux, wl_fixed_t uy) {
    printf("REL %.1f %.1f\n", wl_fixed_to_double(dx), wl_fixed_to_double(dy));
    fflush(stdout);
    if (lock && ++seen >= 6) {
        zwp_locked_pointer_v1_destroy(lock);
        lock = NULL;
        puts("UNLOCKED");
        fflush(stdout);
    }
}
static const struct zwp_relative_pointer_v1_listener rl = {rel};

static void p_enter(void* d, struct wl_pointer* p, uint32_t s, struct wl_surface* f, wl_fixed_t x, wl_fixed_t y) {}
static void p_leave(void* d, struct wl_pointer* p, uint32_t s, struct wl_surface* f) {}
static void p_motion(void* d, struct wl_pointer* p, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    printf("MOTION %.1f %.1f\n", wl_fixed_to_double(x), wl_fixed_to_double(y));
    fflush(stdout);
}
static void p_button(void* d, struct wl_pointer* p, uint32_t s, uint32_t t, uint32_t b, uint32_t st) {
    if (st && !lock && !seen) {
        lock = zwp_pointer_constraints_v1_lock_pointer(cons, surf, ptr, NULL, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
        zwp_locked_pointer_v1_add_listener(lock, &ll, NULL);
        wl_surface_commit(surf);
    }
}
static void p_axis(void* d, struct wl_pointer* p, uint32_t t, uint32_t a, wl_fixed_t v) {}
static void p_frame(void* d, struct wl_pointer* p) {}
static void p_src(void* d, struct wl_pointer* p, uint32_t s) {}
static void p_stop(void* d, struct wl_pointer* p, uint32_t t, uint32_t a) {}
static void p_disc(void* d, struct wl_pointer* p, uint32_t a, int32_t v) {}
static const struct wl_pointer_listener pl = {p_enter, p_leave, p_motion, p_button, p_axis, p_frame, p_src, p_stop, p_disc};

int main(void) {
    struct wl_display*  dpy = wl_display_connect(NULL);
    if (!dpy) return 1;
    struct wl_registry* r = wl_display_get_registry(dpy);
    wl_registry_add_listener(r, &reg, NULL);
    wl_display_roundtrip(dpy);
    if (!comp || !shm || !seat || !wm || !cons || !relmgr) return 2;
    xdg_wm_base_add_listener(wm, &wml, NULL);
    surf = wl_compositor_create_surface(comp);
    struct xdg_surface*  xs = xdg_wm_base_get_xdg_surface(wm, surf);
    xdg_surface_add_listener(xs, &xsl, NULL);
    struct xdg_toplevel* top = xdg_surface_get_toplevel(xs);
    xdg_toplevel_add_listener(top, &tl, NULL);
    xdg_toplevel_set_title(top, "locker");
    wl_surface_commit(surf);
    wl_display_roundtrip(dpy);
    ptr = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(ptr, &pl, NULL);
    struct zwp_relative_pointer_v1* rp = zwp_relative_pointer_manager_v1_get_relative_pointer(relmgr, ptr);
    zwp_relative_pointer_v1_add_listener(rp, &rl, NULL);
    while (wl_display_dispatch(dpy) != -1) {}
    return 0;
}
