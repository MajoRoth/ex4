//
// Created by amitroth on 5/13/23.
//

#include "utils.h"
#include <iostream>


int error(ERR err, const std::string& text){
    if (err == SYS_ERR){
        std::cerr << "system error: " << text <<std::endl;
        exit(1);
    }
    else{
        std::cerr << "thread library error: " << text <<std::endl;
        return -1;
    }
}

uint64_t abs (uint64_t a, uint64_t b){
    if(a - b < 0){
        return b - a;
    }
    return a - b;
}

uint64_t min (uint64_t a, uint64_t b){
    if(a < b){
        return a;
    }
    return b;
}
