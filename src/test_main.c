#include "arena.h"
#include "conn.h"
#include "handler.h"
#include "kqueue.h"

int main()
{
    int r = 0;

    // kqueue.c
    printf("[SUITE]: kqueue.c\n");
    if (kqueue_test_suite() < 0) {
        r = 1;
        printf("\t❌ Suite failed: kqueue.c\n");
    } else {
        printf("\t✅ Suite passed: kqueue.c\n");
    }

    // conn.c
    printf("[SUITE]: conn.c\n");
    if (conn_test_suite() < 0) {
        r = 1;
        printf("\t❌ Suite failed: conn.c\n");
    } else {
        printf("\t✅ Suite passed: conn.c\n");
    }

    // arena.c
    printf("[SUITE]: arena.c\n");
    if (arena_test_suite() < 0) {
        r = 1;
        printf("\t❌ Suite failed: arena.c\n");
    } else {
        printf("\t✅ Suite passed: arena.c\n");
    }

    // handler.c
    printf("[SUITE]: handler.c\n");
    if (handler_test_suite() < 0) {
        r = 1;
        printf("\t❌ Suite failed: handler.c\n");
    } else {
        printf("\t✅ Suite passed: handler.c\n");
    }

    return r;
}
