/*
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjoern Stabell       <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
 *      Dick Balaska         <dick@xpilot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

#include <sys/types.h>

/*
 * A double linked list similar to the STL list, but implemented in C.
 */

/* store a list node. */
struct TListNode_ {
	struct TListNode_ *Next;
	struct TListNode_ *Prev;
	void *Data;
};
typedef struct TListNode_ TListNode;

/* store the list header. */
struct TList_ {
	TListNode Tail;
	int32_t Size;
};
typedef struct TList_ *TList;
typedef struct TListNode_ *TListIter;

/* create a new list and return the new list or NULL on failure. */
TList LIST_New(void);

/* delete a list. */
void LIST_Delete(TList);

void LIST_DeleteNodesOnly(TList l);

/* return a list iterator pointing to the first element of the list. */
TListIter LIST_GetFirstItem(TList);

/* return a list iterator pointing to the one past the last element of the list. */
TListIter LIST_GetLastItem(TList);

/* return a pointer to the last list element. */
void* LIST_GetLastData(TList);

/* return a pointer to the first list element. */
void* LIST_GetFirstData(TList);

/* erase all elements from the list. */
void LIST_Clear(TList);

/* return true if list is empty. */
int32_t LIST_IsEmpty(TList);

/* erase element at list position. */
TListIter LIST_RemoveItem(TList, TListIter);

/* erase a range of list elements excluding last. */
TListIter LIST_RemoveItemRange(TList alist, TListIter first, TListIter last);

/* insert a new element int32_to the list at position
 * and return the new position or NULL on failure. */
TListIter LIST_AddItem(TList alist, TListIter position, void *element);

/* remove the first element from the list and return a pointer to it. */
void* LIST_RemoveFirstItem(TList);

/* remove the last element from the list and return a pointer to it. */
void* LIST_RemoveLastItem(TList);

void LIST_MoveItemTop(TList list, TListIter iter);
void LIST_MoveItemBottom(TList list, TListIter iter);

void LIST_SetIterFirst(TList list, TListIter iter);
void LIST_SetIterLast(TList list, TListIter iter);

/* add a new element to the beginning of the list.
 * and return the new position or NULL on failure. */
TListIter LIST_AddItemTop(TList list, void *data);

/* append a new element at the end of the list.
 * and return the new position or NULL on failure. */
TListIter LIST_AddItemBottom(TList list, void *data);

/*
 * Find an element in the list and return an iterator pointing to it.
 * Note that this is very slow because it traverses the entire list
 * searching for an element.
 */
TListIter LIST_FindItem(TList list, void *data, int32_t offset, int32_t size);

/*
 * Find an element in a range of elements (excluding last) and return
 * an iterator pointing to it.  Note that this is a very slow operation.
 */
TListIter LIST_FindItemRange(TListIter first, TListIter last, void *data, int32_t offset, int32_t size);

/*
 * Remove all element from the list which are equal to Data.
 * Note that this is very slow because it traverses the entire list.
 * The return Value is the number of successful removals.
 */
int32_t LIST_RemoveData(TList list, void *data);

/* return the number of elements in the list. */
int32_t LIST_GetItemCount(TList);

/* advance list iterator one position and return new position. */
TListIter LIST_ForwardIter(TListIter *pos);

/* move list iterator one position backwards and return new position. */
TListIter LIST_BackIter(TListIter *pos);

/* return Data at list position. */
void* LIST_GetItemData(TListIter pos);

int32_t LIST_GetListCount(void);
int32_t LIST_GetNodeCount(void);
void LIST_GetItemsToBuffer(TList list, char *buf, int32_t item_size);
void LIST_AddItemsFromBuffer(TList list, char *buf, int32_t item_size, int32_t num_items);

/* macros to reduce typing. */
#define LI_FORWARD(pos_)	LIST_ForwardIter(&(pos_))
#define LI_BACKWARD(pos_)	LIST_BackIter(&(pos_))
#define LI_DATA(pos_)		LIST_GetItemData((pos_))

#endif
