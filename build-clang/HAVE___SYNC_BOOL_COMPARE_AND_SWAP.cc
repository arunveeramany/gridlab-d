
int main() {
    volatile unsigned int value = 0;
    __sync_bool_compare_and_swap(&value, value, 1);
    return 0;
}