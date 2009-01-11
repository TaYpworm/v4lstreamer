#ifndef __IOEXCEPTION_H__
#define __IOEXCEPTION_H__

#include <exception>

using namespace std;

class IOException: public exception {
public:
    const char *str;

public:
    IOException();
    IOException(const char *exp);
    virtual const char *what() const throw();
};

#endif

