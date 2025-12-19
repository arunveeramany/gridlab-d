
#include <sched.h>
int main() {
    cpu_set_t t;
    CPU_ZERO(&t);
    CPU_SET(1,&t);
    return 0;
}