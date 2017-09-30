#include <stdio.h>
#include <unistd.h>
#include "thrdpool.h"

void test(void *);

int main(void)
{
    thrd_pool_t* pool = pool_create(4);
    add_task(pool, test, NULL);

    sleep(1);
    pool_destroy(pool);
    return 0;
}

void test(void *null) {
    puts("Executing test....");
    puts("Sleep for 3 second...");
    sleep(3);
    puts("Waken");
}
