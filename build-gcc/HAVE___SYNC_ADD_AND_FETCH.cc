
int main() {
    volatile unsigned int value = 0;
    __sync_add_and_fetch(&value, 1);
    return 0;
}