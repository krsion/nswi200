#include <malloc.h>

int main (void) {
    int *p = (int *) malloc (sizeof (int) * 16);
    for (int i = 1 ; i <= 16 ; i ++) {
        p [i] = i;
    }
    free (p);
    return (0);
}
