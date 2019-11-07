#ifndef _LIST_H
#define _LIST_H
#include "type.h"
#define NULL 0
struct NODE{
    void* data;
    int val;
    NODE* prev;
    NODE* next;
    NODE():data(NULL), val(0), prev(NULL), next(NULL){}
    void insert_before(NODE* node);
    void remove();
};


struct LIST{
    NODE* head;
    NODE* tail;
    NODE* ptr;
    uint32 size;
    LIST();
    NODE* pushback(void* data);
    NODE* push_pri(void* data, int val);
    NODE* push_if_not_exist(void* data, int val);
    NODE* find(int val);
    void* getvalue(NODE* node);
    void remove(NODE* node);
    NODE* getNext(NODE* p);
    NODE* wrap(void* data);
};
#endif