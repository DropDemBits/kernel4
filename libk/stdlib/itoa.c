#include <stdlib.h>

char* itoa (int value, char * str, int base)
{ 
    int begin = 0, index = 0;
    
    if(base < 2 || base > 36) return str;
    else if(value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
    else if(value < 0) {
        str[0] = '-';
        begin++;
        value = ~(value)+1;
    }

    for(index = begin; value; index++) {
        str[index] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[value % base];
        value /= base;
    }
    
    str[index--] = '\0';
    
    // Reverse String
    char temp = '\0';
    for(; index > begin; index--, begin++) {
        temp = str[index];
        str[index] = str[begin];
        str[begin] = temp;
    }
    
    return str;
}
