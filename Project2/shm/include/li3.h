#ifndef LI3_H
#define LI3_H

#include <stdio.h>
#include <stdlib.h>

typedef struct LineNode
{
    int linenum;
    char *cmdline;
    int pipeto;      /* recode number after this line to pipe  */
    int pipe_err;    /* boolean, pipe stderr with stdout or not */
    int fd_tofile;   /* fd point to the file this line redirect to */
    char *filename;  /* file name of redirect */
    int is_fifofile; /* boolean, whether the filename is a namepipe file */
    int fd_readout;  /* fd, previous pipe/namepipe to this line, readout end for this line to read */
    int fd_writein;  /* fd, previous pipe to this line, writein end for previous line to write */
    struct LineNode *nextPtr;
} LineNode;

LineNode *create_node(int, char *, int);
void insert_node(LineNode *, LineNode *);
void remove_node(LineNode *);
void print_lists(LineNode *);
void free_lists(LineNode *);

#endif
