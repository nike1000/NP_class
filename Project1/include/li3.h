#ifndef LI3_H
#define LI3_H

#include <stdio.h>
#include <stdlib.h>

typedef struct linenode
{
    int linenum;
    char* cmdline;
    int pipeto;
    int pipe_err;
    int fd_readout;
    int fd_writein;
    struct linenode* nextPtr;
}linenode;

linenode* create_node(int, char*, int);
void insert_node(linenode *, linenode *);
void remove_node(linenode *);
void print_lists(linenode *);
void free_lists(linenode *);

#endif
