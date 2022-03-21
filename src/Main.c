#include <stdio.h>

#include "CommonUtil.h"

int main(int argc, char **argv)
{
    printf("%s %d %d %d\n", argv[0], RIGHT_MOST_ZERO(3), RIGHT_MOST_ZERO(5),
           RIGHT_MOST_ZERO(15));
    return 0;
}
