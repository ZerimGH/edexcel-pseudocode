#ifndef DEF_H
#define DEF_H
#define PERROR(fmt, ...) fprintf(stderr, "(%s): " fmt, __func__, ##__VA_ARGS__)
#endif
