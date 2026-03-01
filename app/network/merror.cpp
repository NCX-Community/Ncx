#include "merror.h"

void errif(bool condition, const char* msg) {
    if (condition) {
        perror(msg);
        exit(1);
    }
}