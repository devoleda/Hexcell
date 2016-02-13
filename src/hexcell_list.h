/*
 * Copyright (c) 2016 Devoleda Organisation. All rights reserved.
 * Copyright (c) Ethan Levy <eitanlevy97@yandex.com>
 *
 * @DEVOLEDA_OSES_BSD_LICENCE_START@
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
 * THIS SOFTWARE IS PROVIDED BY DEVOLEDA ORGANISATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @DEVOLEDA_OSES_BSD_LICENCE_END@
 *
 */
#ifndef _HEXCELL_LIST_H_
#define _HEXCELL_LIST_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { // Status Code Definations
    HEXCELL_LIST_SUCCESS = 0,
    HEXCELL_LIST_ALLOCATION_FAILURE,
    HEXCELL_LIST_NULL_POINTER
};

typedef struct _HListNode {
    void *data;                      // Data held by the node
    struct _HListNode *previous;     // Pointer of previous node
    struct _HListNode *next;         // Pointer of next node
} HListNode;

typedef struct _HListHandle {
    unsigned int nodes;              // The count of nodes
    HListNode *head;                 // Pointer of first node of the list
} HList;

/* List data comparison helper function */
typedef int (*HListCompareHelper)(void *, void *);

/* Data Destroy Helper Function */
typedef void (*HListDestroyHelper)(void *);

int HListErrno(void);                // NOT SECURE UNDER MULTITHREADS

/* Allocations */
HList *HListNew(void *dataInitial);
void HListDestroyAdvanced(HList **pList, HListDestroyHelper fnHelper);
#define HListDestroy(p) HListDestroyAdvanced(p, NULL)

/* Node Mutators */
HList *HListNodeAdd(HList *pList, void *pData);
HList *HListNodeInsert(HList *pList, void *pData, HListNode *pNode);
HList *HListNodeInsertByIndex(HList *pList, void *pData, unsigned int inIndex);

HList *HListNodeRemoveAdvanced(HList *pList, HListNode *pNode, HListDestroyHelper fnHelper);
#define HListNodeRemove(l, n) HListNodeRemoveAdvanced(l, n, NULL);
HList *HListNodeRemoveByIndexAdvanced(HList *pList, unsigned int inIndex, HListDestroyHelper fnHelper);
#define HListNodeRemoveByIndex(l, i) HListNodeRemoveByIndexAdvanced(l, i, NULL);
HList *HListNodeRemoveByDataAdvanced(HList *pList, void *pData, HListCompareHelper fnHelper, 
    HListDestroyHelper fnDestroyHelper);
#define HListNodeRemoveByData(l, d, c) HListNodeRemoveByDataAdvanced(l, d, c, NULL);

/* Node Accessors */
HListNode *HListNodeLast(HList *pList);
HListNode *HListNodeByIndex(HList *pList, unsigned int inIndex);
HListNode *HListNodeNext(HListNode *pNode);
HListNode *hListNodePrevious(HListNode *pNode);

/* misc */
unsigned int HListNodeCount(HList *pList);
unsigned int HListNodeRealCount(HList *pList);
HListNode *HListNodeFindByData(HList *pList, void *pData, HListCompareHelper fnHelper);
HListNode *HListNodeFindByPointer(HList *pList, void *pPtr);
HListNode *HListNodeFindByString(HList *pList, char *pStr);
HListNode *HListNodeFindByInteger(HList *pList, int *i);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _HEXCELL_LIST_H_ */