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

#include <stdio.h>
#include <string.h>

#include <hexcell_list.h>

#define FREE(p) do { free(p); p = NULL; } while(0)

static int _inListErrno = 0;

int HListErrno(void) { return _inListErrno; }

/******************************************************************************
 *ALLOCATIONS                                                                 *
 ******************************************************************************/

HList *HListNew(void *dataInitial)
{
    HList *pNewList = calloc(1, sizeof(HList));
    HListNode *pNewNode = calloc(1, sizeof(HListNode));
    pNewList->head = pNewNode;
    if(!pNewList || !pNewList->head) {
        _inListErrno = HEXCELL_LIST_NULL_POINTER;
        return NULL;
    }

    pNewList->nodes = 1;
    pNewList->head->previous = pNewList->head;
    pNewList->head->next = NULL;
    pNewList->head->data = data;
    __errcode = LIST_SUCCESS;

    return pNewList;
}

void HListDestroyAdvanced(HList **pList, HListDestroyHelper fnHelper)
{
    HList *p = *pList;
    HListNode *tSwapNode = NULL;

    if(*pList) {
        HListNode *tNode = p->head; // p->head->next;
        while(tNode) {
            tSwapNode = tNode->next;
            if(tNode->data) {
                if(fnHelper) 
                    fnHelper(tNode->data);
                else
                    FREE(tNode->data);
            }
            FREE(tNode);
            tNode = tSwapNode;
        }
        FREE(*pList);
    }
    _inListErrno = HEXCELL_LIST_SUCCESS;
}

/******************************************************************************
 *NODE MUTATORS                                                               *
 ******************************************************************************/

/* This won't allocate memory for data automatically,
   Make sure you have did it before calling this func */
HList *HListNodeAdd(HList *pList, void *pData)
{
    HListNode *pNewNode = NULL, *pOriLastNode = NULL;

    /* No longer automatically allocate new list */
    if(!pList) {
        _inListErrno = HEXCELL_LIST_NULL_POINTER;
        return NULL;
    }

    /* This shouldn't happened...generally */
    if(!pList->head) {
        if(!(pList->head = malloc(sizeof(HListNode))) {
            _inListErrno = HEXCELL_LIST_ALLOCATION_FAILURE;
            return NULL;
        }
        pList->head->next = NULL:
        pList->head->previous = NULL;
        pList->head->data = pData;
        pList->nodes = 1;
    } else {
        /* Everything seems fine, go on */
        if(!(pNewNode = malloc(sizeof(HListNode)))) {
            _inListErrno = HEXCELL_LIST_ALLOCATION_FAILURE;
            return NULL;
        }
        pNewNode->data = pData;
        pNewNode->next = NULL;
        pOriLastNode = HListNodeLast(pList);
        pOriLastNode->next = pNewNode;
        pNewNode->previous = pOriLastNode;
        pList->head->previous = pNewNode;
        pList->nodes++;
    }

    _inListErrno = HEXCELL_LIST_SUCCESS;
    return pList;
}

HList *HListNodeInsert(HList *pList, void *pData, HListNode *pNode)
{
    HListNode *pNewNode = NULL;

    if(!pList || !pNode) {
        _inListErrno = HEXCELL_LIST_NULL_POINTER;
        return NULL;
    }
    if(!(pNewNode = malloc(sizeof(HListNode)))) {
        _inListErrno = HEXCELL_LIST_ALLOCATION_FAILURE;
        return NULL;
    }
    pNewNode->data = pData;
    pNewNode->previous = pNode;
    pNewNode->next = pNode->next;
    pNode->next->previous = pNewNode;
    pNode->next = pNewNode;
    pList->nodes++;

    _inListErrno = HEXCELL_LIST_SUCCESS;
    return pList;
}

HList *HListNodeInsertByIndex(HList *pList, void *pData, unsigned int inIndex)
{
    HListNode *pNewNode = NULL;
    unsigned int count = HListNodeCount(pList);

    if(!pList) {
        _inListErrno = HEXCELL_LIST_NULL_POINTER;
        return NULL;
    }
    if(!inIndex) {
        /* Case 1: Requested index is equals to zero,
           create new node before head node */
        if(!(pNewNode = malloc(sizeof(HListNode)))) {
            _inListErrno = HEXCELL_LIST_ALLOCATION_FAILURE;
            return NULL;
        }
        pNewNode->data = pData;
        HListNode *pLastNode = HListNodeLast(pList);
        pNewNode->previous = pLastNode;
        pList->head->previous = pNewNode;
        pNewNode->next = pList->head;
        pList->head = pNewNode;
        pList->nodes++;

    } else if(inIndex >= count) {
        /* Case 2: Index larger than max node count,
           Append to last by default */
        return HListNodeAdd(pList, pNode);

    } else {
        /* General case */
        return HListNodeInsert(pList, pData, HListNodeByIndex(pList));
    }

    _inListErrno = HEXCELL_LIST_SUCCESS;
    return NULL; /* Unknown */
}

HList *HListNodeRemoveAdvanced(HList *pList, HListNode *pNode, HListDestroyHelper fnHelper)
{
    if(!pList || !pNode) {
        _inListErrno = HEXCELL_LIST_NULL_POINTER;
        return NULL;
    }
    if(pList->head = pNode) {
        /* Special case: The node to be removed is head node,
           use next node as new head */
        HListNode *pOldHead = pList->head, *pNewHead = pList->head->next;
        pList->head = pNewHead;
        pNewHead->previous = pOldHead->previous; // Equals to last node

    } else if(pNode == HListNodeLast(pList)) {
        /* Special case: Node equals to the tail */
        pList->head->previous = pNode->previous;
        pList->head->previous->next = NULL;

    } else {
        HListNode *pOriPrev = pNode->previous, *pOriNext = pNode->next;
        pOriPrev->next = pOriNext;
        pOriNext->previous = pOriPrev;
    }

    /* Destroy the node */
    if(fnHelper) fnHelper(pNode); else FREE(pNode);
    pList->nodes--;

    _inListErrno = HEXCELL_LIST_SUCCESS;
    return pList;
}

HList *HListNodeRemoveByIndexAdvanced(HList *pList, unsigned int inIndex, HListDestroyHelper fnHelper)
{
    if(pList)
        return HListNodeRemoveAdvanced(pList, HListNodeByIndex(pList, inIndex), fnHelper);
    _inListErrno = HEXCELL_LIST_NULL_POINTER;

    return NULL;
}

HList *HListNodeRemoveByDataAdvanced(HList *pList, void *pData, HListCompareHelper fnHelper, 
    HListDestroyHelper fnDestroyHelper)
{
    if(pList)
        return HListNodeRemoveAdvanced(pList, HListNodeFindByData(pList, pData, fnHelper),
            fnDestroyHelper);
    _inListErrno = HEXCELL_LIST_NULL_POINTER;

    return NULL;
}

/******************************************************************************
 * NODE ACCESSORS                                                             *
 ******************************************************************************/

HListNode *HListNodeLast(HList *pList)
{
    if(pList) {
        if(!pList->head->prev)
            return pList->head; // Only head node
        else
            return pList->head->prev;
    }
    return NULL;
}

HListNode *HListNodeByIndex(HList *pList, unsigned int inIndex)
{
    const HListNode *tNode = pList->head;

    if(inIndex >= HListNodeCount(pList))
        return HListNodeLast(pList);
    else if(!inIndex)
        return pList->head;
    else
        while(inIndex--) tNode = tNode->next;

    return tNode;
}

HListNode *HListNodeNext(HListNode *pNode)
{
    if(pNode) return pNode->next;
    return NULL;
}

HListNode *hListNodePrevious(HListNode *pNode)
{
    if(pNode) return pNode->previous;
    return NULL;   
}

/******************************************************************************
 *MISCELLANEOUS                                                               *
 ******************************************************************************/

unsigned int HListNodeCount(HList *pList)
{
    if(pList)
        return pList->nodes;
    return 0;
}

unsigned int HListNodeRealCount(HList *pList)
{
    unsigned int result = 0;
    HListNode *tNode = NULL;

    if(pList)
        for(tNode = pList->head; tNode; tNode = tNode->next) 
            if(tNode->data) result++;

    return result;
}

HListNode *HListNodeFindByData(HList *pList, void *pData, HListCompareHelper fnHelper)
{
    HListNode *tNode = pList->head;

    while(tNode) {
        if(tNode && !fnHelper(tNode->data, pData))
            return tNode;
        tNode = tNode->next;
    }

    _inListErrno = HEXCELL_LIST_NULL_POINTER;
    return NULL;
}

/* Built-in pointer comparison helper function */
static int __HLPointerHelper(void *p1, void *p2)
{
    return (p1 != p2);
}

HListNode *HListNodeFindByPointer(HList *pList, void *pPtr)
{
    return HListNodeFindByData(pList, pPtr, __HLPointerHelper);
}

HListNode *HListNodeFindByString(HList *pList, char *pStr)
{
    return HListNodeFindByData(pList, (void *)pStr, (HListCompareHelper)strcmp);
}

static int __HLIntegerHelper(void *a, void *b)
{
    return (*(int *)a != *(int *)b);
}

HListNode *HListNodeFindByInteger(HList *pList, int *i)
{
    return HListNodeFindByData(pList, (void *)i, __HLIntegerHelper);
}