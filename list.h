/*****************************************************/
/*
**   Author: lirui
**   Date: 2023/09/20
**   File: IList.h
**   Function:  Interface of List for user
**   History:
**       2023/09/20 create by lirui
**
**   Copy Right: lirui
**
*/
/*****************************************************/
#ifndef INTERFACE_LIST_NODE_H
#define INTERFACE_LIST_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct list_head {
    struct list_head *prev;
    struct list_head *next;
} list_head;


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
struct list_head name = LIST_HEAD_INIT(name)


/**
* list_entry get the struct for this entry
* @ptr:    the &struct list_head pointer.
* @type:    the type of the struct this is embedded in.
* @member:    the name of the list_struct within the struct.
*/

#define list_entry(ptr, type, member) \
((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


void INIT_LIST_HEAD(struct list_head *list);

void list_add(struct list_head *new, struct list_head *head);

void list_add_tail(struct list_head *new, struct list_head *head);

void list_del(struct list_head *entry);

int list_empty(struct list_head *head);

//list -> next  all nodes
#define list_for_each(pos, head) \
for (pos = (head)->next; pos != (head); \
pos = pos->next)

//list -> prev  all nodes
#define list_for_each_prev(pos, head) \
for (pos = (head)->prev; pos != (head); \
pos = pos->prev)

//list -> next  all nodes  safe
#define list_for_each_safe(pos, n, head) \
for (pos = (head)->next, n = pos->next; pos != (head); \
pos = n, n = pos->next)

//list struct -> next  all nodes struct
#define list_for_each_entry(pos, head, member)                \
for (pos = list_entry((head)->next, typeof(*pos), member);    \
&pos->member != (head);                     \
pos = list_entry(pos->member.next, typeof(*pos), member))

//list struct -> next  all nodes struct safe
#define list_for_each_entry_safe(pos, n, head, member)            \
for (pos = list_entry((head)->next, typeof(*pos), member),    \
n = list_entry(pos->member.next, typeof(*pos), member);    \
&pos->member != (head);                     \
pos = n, n = list_entry(n->member.next, typeof(*n), member))

#ifdef __cplusplus
}
#endif

#endif