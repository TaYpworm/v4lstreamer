#include "IOException.h"

#include <cstdio>

IOException::IOException() {
    str = "Unspecified";
}

IOException::IOException(const char *exp) {
    str = exp;
}

const char *IOException::what() const throw() {
    return str;
}
