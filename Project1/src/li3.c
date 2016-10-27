#include "../include/li3.h"

linenode* create_node(int linenum, char* data, int pipeto)
{
    linenode* n = (linenode*)malloc(sizeof(linenode));
    n->linenum = linenum;
	n->cmdline = data;
    n->pipeto = pipeto;
    n->pipe_err = 0;
    n->fd_writein = -1;
    n->fd_readout = -1;
    n->nextPtr = NULL;
    return n;
}

void insert_node(linenode* n1, linenode* n2)
{
    n2->nextPtr = n1->nextPtr;
    n1->nextPtr = n2;
}

void remove_node(linenode* n1)
{
    n1->nextPtr = n1->nextPtr->nextPtr;
}

void print_lists(linenode* lists)
{
    linenode* n = lists;
    
    while (n != NULL)
    {
        printf("%d:%s,%d ", n->linenum, n->cmdline, n->pipeto);
        n = n->nextPtr;
    }

    printf("\n");
}

void free_lists(linenode* lists)
{
    // 遞迴刪除串列所有節點
    if (lists->nextPtr != NULL)
    {
        free_lists(lists->nextPtr);
    }

    free(lists);
}
