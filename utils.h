//
// Created by amitroth on 5/13/23.
//
#include <iostream>

#ifndef OS_EX3_UTILS_H
#define OS_EX3_UTILS_H

enum ERR{SYS_ERR, LIB_ERR};


int error(ERR err, const std::string& text);

uint64_t abs (uint64_t a, uint64_t b);

uint64_t min (uint64_t a, uint64_t b);



#endif //OS_EX3_UTILS_H
