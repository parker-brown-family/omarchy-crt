// Test rig: a mouse for a seat with no mouse. Reads "move X Y" and "click" from a
// FIFO and sends them through the virtual pointer protocol, the same input path a
// real device takes, so a click here is a real click to the compositor.
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <wayland-client.h>
#include "vp.h"

static struct zwlr_virtual_pointer_manager_v1* mgr;
static struct wl_seat*                          seat;
static void on_global(void* d, struct wl_registry* r, uint32_t id, const char* name, uint32_t v) {
    if (!strcmp(name, "zwlr_virtual_pointer_manager_v1")) mgr = wl_registry_bind(r, id, &zwlr_virtual_pointer_manager_v1_interface, 1);
    if (!strcmp(name, "wl_seat")) seat = wl_registry_bind(r, id, &wl_seat_interface, 1);
}
static void on_remove(void* d, struct wl_registry* r, uint32_t id) {}
static uint32_t now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
int main(int argc, char** argv) {
    struct wl_display* d = wl_display_connect(NULL);
    if (!d) return 1;
    struct wl_registry* r = wl_display_get_registry(d);
    static const struct wl_registry_listener l = {on_global, on_remove};
    wl_registry_add_listener(r, &l, NULL);
    wl_display_roundtrip(d);
    if (!mgr) return 2;
    struct zwlr_virtual_pointer_v1* p = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(mgr, seat);
    wl_display_roundtrip(d);
    mkfifo(argv[1], 0600);
    char line[64];
    for (;;) {
        FILE* f = fopen(argv[1], "r");
        while (f && fgets(line, sizeof line, f)) {
            unsigned x, y;
            if (sscanf(line, "move %u %u", &x, &y) == 2) {
                zwlr_virtual_pointer_v1_motion_absolute(p, now(), x, y, 1600, 900);
                zwlr_virtual_pointer_v1_frame(p);
            } else if (!strncmp(line, "click", 5)) {
                zwlr_virtual_pointer_v1_button(p, now(), BTN_LEFT, 1);
                zwlr_virtual_pointer_v1_frame(p);
                wl_display_roundtrip(d);
                zwlr_virtual_pointer_v1_button(p, now(), BTN_LEFT, 0);
                zwlr_virtual_pointer_v1_frame(p);
            }
            wl_display_roundtrip(d);
        }
        if (f) fclose(f);
    }
}
