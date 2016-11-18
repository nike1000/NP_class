#ifndef LI3_H
#define LI3_H

#include <stdio.h>
#include <stdlib.h>

typedef struct LineNode
{
    int linenum;
    char* cmdline;
    int pipeto;
    int pipe_err;
    int fd_tofile;
    char* filename;
    int is_fifofile;
    int fd_readout;
    int fd_writein;
    struct LineNode* nextPtr;
}LineNode;

LineNode* create_node(int, char*, int);
void insert_node(LineNode *, LineNode *);
void remove_node(LineNode *);
void print_lists(LineNode *);
void free_lists(LineNode *);

#endif
