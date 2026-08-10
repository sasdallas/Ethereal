/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * 
 * Copyright (c) 2026
 *  Samuel Stuart
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* This is a re-implementation of the FreeBSD SLIST/STAILQ/DLIST API */

#ifndef STRUCTS_LIST_NEW_H
#define STRUCTS_LIST_NEW_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

/**** DEFINITIONS ****/

/* NOTE: These aren't safe if evaluating with changing arguments */

#define SLIST_HEAD(name, type) \
struct {\
    type *first;\
} name

#define SLIST_HEAD_INITIALIZER { NULL }
#define SLIST_HEAD_INIT(head) ({ (head)->first = NULL; })

#define SLIST_ENTRY(type) \
struct {\
    type *next;\
}

#define SLIST_EMPTY(head) ((head)->first == NULL)
#define SLIST_FIRST(head) ((head)->first)
#define SLIST_NEXT(elm, field) ((elm)->field.next)

#define SLIST_FOREACH(type, name, head, field) \
for (type * (name) = SLIST_FIRST(head); (name); (name) = SLIST_NEXT(name, field))

#define SLIST_INSERT_HEAD(head, elm, field) ({\
    SLIST_NEXT(elm, field) = SLIST_FIRST(head);\
    SLIST_FIRST(head) = (elm);\
})

#define SLIST_INSERT_AFTER(node, elm, field) ({\
    SLIST_NEXT(elm, field) = SLIST_NEXT(node, field);\
    SLIST_NEXT(node, field) = (elm);\
})

#define SLIST_REMOVE_HEAD(head, field) ({\
    SLIST_FIRST(head) = SLIST_NEXT(SLIST_FIRST(head), field);\
})

#define SLIST_REMOVE(head, elm, type, field) ({\
    if (SLIST_FIRST(head) == (elm)) {\
        SLIST_FIRST(head) = SLIST_NEXT(SLIST_FIRST(head), field);\
    } else {\
        SLIST_FOREACH(type, __iter, head, field) {\
            if (SLIST_NEXT(__iter,field) == (elm)) {\
                SLIST_NEXT(__iter,field) = SLIST_NEXT(elm,field);\
                break;\
            }\
        }\
    }\
})

/* STAILQ */

#define STAILQ_HEAD(name, type) \
struct {\
    type *first;\
    type **last;\
} name

#define STAILQ_ENTRY(type) \
struct {\
    type *next;\
}

#define STAILQ_FIRST(tailq) ((tailq)->first)
#define STAILQ_LAST(tailq) (*(tailq)->last)
#define STAILQ_NEXT(elem, field) ((elem)->field.next)

#define STAILQ_INITIALIZER(name) { .first = NULL, .last = &(name).first }
#define STAILQ_INIT(name) ({ (name)->first = NULL; (name)->last = &(name)->first; })

#define STAILQ_INSERT_HEAD(name, elem, field) ({\
    STAILQ_NEXT(elem, field) = STAILQ_FIRST(name);\
    if (STAILQ_FIRST(name) == NULL) {\
        (name)->last = &(elem)->field.next;\
    }\
    STAILQ_FIRST(name) = (elem);\
})

#define STAILQ_INSERT_AFTER(head, after, elem, field) ({\
    if (STAILQ_NEXT(after, field) == NULL) {\
        (head)->last = &(elem)->field.next; \
    }\
    STAILQ_NEXT(elem, field) = STAILQ_NEXT(after, field);\
    STAILQ_NEXT(after, field) = (elem);\
})

#define STAILQ_INSERT_TAIL(head, elem, field) ({\
    STAILQ_LAST(head) = elem;\
    STAILQ_NEXT(elem, field) = NULL;\
    (head)->last = &(elem)->field.next;\
})

#define STAILQ_REMOVE_HEAD(head, field) ({\
    if ((STAILQ_FIRST((head)) = STAILQ_NEXT(STAILQ_FIRST((head)), field)) == NULL) {\
        (head)->last = &(head)->first; \
    }\
})

/* TODO: STAILQ_REMOVE, STAILQ_FOREACH, etc. */

/* DLIST */
#define DLIST_HEAD(name, type) \
struct {\
    type *first;\
    type *last;\
} name

#define DLIST_ENTRY(type) \
struct {\
    type *next;\
    type *prev;\
}

#define DLIST_INIT(val) ({ (val)->first = NULL; (val)->last = NULL; })

#define DLIST_FIRST(list) ((list)->first)
#define DLIST_LAST(list) ((list)->last)
#define DLIST_NEXT(node, field) ((node)->field.next)
#define DLIST_PREV(node, field) ((node)->field.prev)

#define DLIST_INSERT_HEAD(list, node, field) ({\
    DLIST_NEXT(node, field) = DLIST_FIRST(list);\
    DLIST_PREV(node, field) = NULL; \
    if (DLIST_FIRST(list)) {\
        DLIST_PREV(DLIST_FIRST(list), field) = (node);\
    } else {\
        DLIST_LAST(list) = DLIST_FIRST(list);\
    }\
    DLIST_FIRST(list) = (node);\
})

#define DLIST_INSERT_AFTER(list, after, node, field) ({\
    DLIST_NEXT(node, field) = DLIST_NEXT(after, field);\
    DLIST_NEXT(after, field) = (node);\
    DLIST_PREV(node, field) = (after);\
    if (DLIST_NEXT(node, field) != NULL) {\
        DLIST_PREV(DLIST_NEXT(node, field), field) = (node);\
    } else {\
        DLIST_LAST(list) = (node);\
    }\
})

#define DLIST_INSERT_TAIL(list, node, field) ({\
    DLIST_NEXT(node, field) = NULL;\
    DLIST_PREV(node, field) = DLIST_LAST(list);\
    if (DLIST_LAST(list)) {\
        DLIST_NEXT(DLIST_LAST(list), field) = (node);\
    } else {\
        DLIST_FIRST(list) = (node);\
    }\
    DLIST_LAST(list) = (node);\
}) 

#define DLIST_REMOVE(list, type, node, field) ({\
    type *_next = DLIST_NEXT(node, field);\
    type *_prev = DLIST_PREV(node, field);\
    if (_prev != NULL) {\
        DLIST_NEXT(_prev, field) = _next;\
    } else {\
        DLIST_FIRST(list) = _next;\
    }\
    if (_next != NULL) {\
        DLIST_PREV(_next, field) = _prev;\
    } else {\
        DLIST_LAST(list) = _prev;\
    }\
    DLIST_NEXT(node, field) = NULL;\
    DLIST_PREV(node, field) = NULL;\
})

#define DLIST_FOREACH(type, name, head, field) \
for (type * (name) = DLIST_FIRST(head); (name); (name) = DLIST_NEXT(name, field))

#endif
