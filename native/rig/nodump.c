// Test rig only. Every program in the seat marks itself non-dumpable, because a core
// dump anywhere on this box reaches omarchy-crash-watch, which raises a "Process
// crashed" toast on the live desktop. A core limit does not stop that here: on this
// kernel a limit of 1 still dumps to a piped core_pattern (measured 2026-11-04).
// Dumpable is reset on exec, so it has to be set again in each program, by preload.
#include <sys/prctl.h>
__attribute__((constructor)) static void nodump(void) { prctl(PR_SET_DUMPABLE, 0); }
