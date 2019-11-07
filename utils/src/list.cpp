#include "list.h"
#include <stdlib.h>

void NODE::remove(){
    NODE* p = this->prev;
    NODE* n = this->next;
    if(p){
        p->next = n;
        this->prev = NULL;
    }
    if(n){
        n->prev = p;
        this->next = NULL;
    }
}

void NODE::insert_before(NODE* node){
    node->prev = this->prev;
    if(this->prev){
        this->prev->next = node;
    }
    node->next = this;
    this->prev = node;
}

LIST::LIST(){
    this->head = new NODE;
    this->tail = new NODE;
    this->tail->insert_before(this->head);
    this->ptr = head;
    this->size = 0;
}

NODE* LIST::pushback(void* data){
    NODE* p = new NODE;
    p->data = data;
    this->tail->insert_before(p);
    ++this->size;
    return p;
}

NODE* LIST::push_pri(void* data, int val){
    NODE* p = new NODE;
    p->data = data;
    p->val = val;
    ++this->size;
    NODE* q = head->next;
    while(q != tail){
        if(p->val > q->val){
            q = q->next;
        }else{
            q->insert_before(p);
            return p;
        }
    }
    this->tail->insert_before(p);
    return p;
}

NODE* LIST::find(int val){
    NODE* q = head->next;
    while(q != tail){
        if(val > q->val){
            q = q->next;
        }else if(val < q->val){
            return NULL;
        }else{
            return q;
        }
    }
    return NULL;
}

NODE* LIST::push_if_not_exist(void* data, int val){
    NODE* q = head->next;
    while(q != tail){
        if(val > q->val){
            q = q->next;
        }else if(val < q->val){
            NODE* p = new NODE;
            p->data = data;
            p->val = val;
            ++this->size;
            q->insert_before(p);
            return p;
        }else{
            return NULL;
        }
    }
    NODE* p = new NODE;
    p->data = data;
    p->val = val;
    ++this->size;
    this->tail->insert_before(p);
    return p;
}

void* LIST::getvalue(NODE* node){
    if(node && node != this->head && node != this->tail){
        return node->data;
    }else{
        return NULL;
    }
}

void LIST::remove(NODE* node){
    if(node && node != this->tail){
        node->remove();
        delete node;
        --this->size;
    }
}

NODE* LIST::getNext(NODE* p){
    if(p->next && p->next != this->tail){
        return p->next;
    }else{
        return NULL;
    }
}

NODE* LIST::wrap(void* data){
    NODE* p = new NODE;
    p->data = data;
}