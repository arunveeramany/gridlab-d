
#include <sched.h>
int main() {
    cpu_set_t *cpu = CPU_ALLOC(2);
    return 0;
}