/* Tests the exit system call. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
    printf("-------------\n");
    printf("-------------\n");
    printf("-------------\n");
    printf("-------------\n");
  exit (57);
  fail ("should have called exit(57)");
}
