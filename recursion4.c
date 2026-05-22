#include <stdio.h>
int test (int a, int b, int c, int d) {
    printf ("%p\n", &a);
    if (a > 0) test (a - 1, b, c, d);
    return (a);
}
int main (void) {
    test (10, 20, 30, 40);
}
