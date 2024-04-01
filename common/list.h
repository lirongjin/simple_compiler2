#ifndef LIST_H
#define LIST_H

typedef struct list_head {
    struct list_head *prev, *next;
} List;

static inline void __list_add(struct list_head *new, struct list_head *prev, struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

#define _offset(type, member)   ((void *)&(((type *)0)->member) - (void *)0)
#define container_of(ptr, type, member) ((type *)((void *)(ptr) - _offset(type, member)))
#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)
#define list_for_each_prev(pos, head) for (pos = (head)->prev; pos != (head); pos = pos->prev)
#define list_empty(list)     ((list)->prev == (list))
#define LIST_HEAD_INIT(list)    {&(list), &(list)}

#define node_is_tail(list, node)    ((list) && (node) && ((node)->next == (list)))
#define node_is_head(list, node)    ((list) && (node) && ((node)->prev == (list)))
#define list_get_head(list)         (((list)->next == (list)) ? NULL : (list)->next)
#define list_get_tail(list)         (((list)->prev == (list)) ? NULL : (list)->prev)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->prev = list;
    list->next = list;
}

static inline void list_del(struct list_head *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

#endif // LIST_H
