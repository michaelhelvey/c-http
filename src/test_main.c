#include "common.h"
#include "conn.h"
#include "kqueue.h"

int main()
{
    int r = 0;

    // kqueue.c
    println("[SUITE]: kqueue.c");
    if (kqueue_test_suite() < 0) {
        r = 1;
        println("\t❌ Suite failed: kqueue.c");
    } else {
        println("\t✅ Suite passed: kqueue.c");
    }

    // conn.c
    println("[SUITE]: conn.c");
    if (conn_test_suite() < 0) {
        r = 1;
        println("\t❌ Suite failed: conn.c");
    } else {
        println("\t✅ Suite passed: conn.c");
    }

    return r;
}
