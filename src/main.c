#include <stdio.h>

extern void common_foo();

int main()
{
    common_foo();
    printf("(main): Hello, World!\n");
    return 0;
}
