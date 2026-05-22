#include <stdio.h>
int test (int a, int b, int c, int d, int e) {
    printf ("%p\n", &a);
    if (a > 0) test (a - 1, b, c, d, e);
    return (a);
}
int main (void) {
    test (10, 20, 30, 40, 50);
}
