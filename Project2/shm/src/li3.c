#include "../include/li3.h"

LineNode *create_node(int linenum, char *data, int pipeto)
{
    LineNode *n = (LineNode *)malloc(sizeof(LineNode));
    n->linenum = linenum;
    n->cmdline = data;
    n->pipeto = pipeto;
    n->pipe_err = 0;
    n->fd_tofile = -1;
    n->filename = NULL;
    n->is_fifofile = 0;
    n->fd_writein = -1;
    n->fd_readout = -1;
    n->nextPtr = NULL;
    return n;
}

void insert_node(LineNode *n1, LineNode *n2)
{
    n2->nextPtr = n1->nextPtr;
    n1->nextPtr = n2;
}

void remove_node(LineNode *n1)
{
    n1->nextPtr = n1->nextPtr->nextPtr;
}

void print_lists(LineNode *lists)
{
    LineNode *n = lists;

    while (n != NULL)
    {
        printf("%d:%s,%d ", n->linenum, n->cmdline, n->pipeto);
        n = n->nextPtr;
    }

    printf("\n");
}

void free_lists(LineNode *lists)
{
    // 遞迴刪除串列所有節點
    if (lists->nextPtr != NULL)
    {
        free_lists(lists->nextPtr);
    }

    free(lists);
}
