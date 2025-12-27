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

#include <string.h>

#include "list.h"
#include "types.h"
#include "debug.h"

static int32_t LIST_NumLists;
static int32_t LIST_NumNodes;

/* create a new list. */
TList LIST_New(void)
{
	TList l = (TList) malloc(sizeof(*l));

	if (l) {
		LIST_NumLists++;

		l->Tail.Next = &l->Tail;
		l->Tail.Prev = &l->Tail;
		l->Tail.Data = NULL;
		l->Size = 0;
	}

	return l;
}

/* delete a list and frees memory used by the data stored on that list. */
void LIST_Delete(TList l)
{
	ASSERT(l)

	if (l) {
		LIST_Clear(l);
		l->Tail.Next = l->Tail.Prev = NULL;
		free(l);

		LIST_NumLists--;
	}
}

/* Delete the list, but do not deallocate the data itself
 *
 * This function is useful if the list stores pointers to elements, that
 * are on some other list too and must not be removed just yet.
 */
void LIST_DeleteNodesOnly(TList l)
{
	ASSERT(l)

	if (l) {
		l->Tail.Next = l->Tail.Prev = NULL;
		free(l);

		LIST_NumLists--;
	}
}

/* return a list iterator pointing to the first element of the list. */
TListIter LIST_GetFirstItem(TList list)
{
	ASSERT(list)

	return list->Tail.Next;
}

/* return a list iterator pointing to the one past the last element of the list. */
TListIter LIST_GetLastItem(TList list)
{
	ASSERT(list)

	return &list->Tail;
}

/* return a pointer to the last list element. */
void* LIST_GetLastData(TList list)
{
	ASSERT(list)

	return list->Tail.Prev->Data;
}

/* return a pointer to the first list element. */
void* LIST_GetFirstData(TList list)
{
	ASSERT(list)

	return list->Tail.Next->Data;
}

/* erase all elements from the list. */
void LIST_Clear(TList list)
{
	void *d;

	ASSERT(list)

	while (!LIST_IsEmpty(list)) {
		d = LIST_RemoveFirstItem(list);
		free(d);
	}
}

/* return true if list is empty. */
int32_t LIST_IsEmpty(TList list)
{
	ASSERT(list)

	return (list->Size == 0);
}

/* erase element at list position. */
TListIter LIST_RemoveItem(TList list, TListIter pos)
{
	TListIter next, prev;
	
        ASSERT(list && pos)

	if (pos == &list->Tail) {
		return LIST_GetLastItem(list);
	}

	next = pos->Next;
	prev = pos->Prev;
	prev->Next = next;
	next->Prev = prev;
	list->Size--;

	pos->Prev = NULL;
	pos->Next = NULL;
	pos->Data = NULL;
	free(pos);

	LIST_NumNodes--;

	return next;
}

/* erase a range of list elements excluding last. */
TListIter LIST_RemoveItemRange(TList list, TListIter first, TListIter last)
{
	ASSERT(list && first && last)

	while (first != last) {
		first = LIST_RemoveItem(list, first);
		free(first->Data);
	}
	return first;
}

/* insert a new element into the list at position
 * and return new position or NULL on failure.
 *
 * NOTE: it is a good practise to allocate the data pointed to by \sa data
 * dynamically, so that it is valid only until removed manually. */
TListIter LIST_AddItem(TList list, TListIter pos, void *data)
{
	TListIter node = (TListIter) malloc(sizeof(*node));

	ASSERT(list && pos && data)

	if (node) {
		node->Next = pos;
		node->Prev = pos->Prev;
		node->Data = data;
		node->Prev->Next = node;
		node->Next->Prev = node;
		list->Size++;

		LIST_NumNodes++;
	}

	return node;
}

/* add a new element to the beginning of the list.
 * and return the new position or NULL on failure. */
TListIter LIST_AddItemTop(TList list, void *data)
{
	ASSERT(list && data)

	return LIST_AddItem(list, list->Tail.Next, data);
}

/* append a new element at the end of the list.
 * and return the new position or NULL on failure. */
TListIter LIST_AddItemBottom(TList list, void *data)
{
	ASSERT(list && data)

	return LIST_AddItem(list, &list->Tail, data);
}

/* remove the first element from the list and return a pointer to it. */
void* LIST_RemoveFirstItem(TList list)
{
	void *data;

	ASSERT(list)

	data = list->Tail.Next->Data;
	LIST_RemoveItem(list, list->Tail.Next);
	return data;
}

/* remove the last element from the list and return a pointer to it. */
void* LIST_RemoveLastItem(TList list)
{
	void *data;

	ASSERT(list)

	data = list->Tail.Prev->Data;
	LIST_RemoveItem(list, list->Tail.Prev);
	return data;
}

//! Moves an item to the top of the list
/*!
 * \param list	list
 * \param iter	iterator pointing to the item, that should be moved
 */
void LIST_MoveItemTop(TList list, TListIter iter)
{
	TListIter old_top = list->Tail.Next;

	ASSERT(list && iter)

	// Move the item if not already on top
	if (iter->Prev != &list->Tail) {
		iter->Prev->Next = iter->Next;
		iter->Next->Prev = iter->Prev;

		iter->Next = old_top;
		old_top->Prev = iter;

		iter->Prev = &list->Tail;
		list->Tail.Next = iter;
	}
}

//! Moves an item to the bottom of the list
/*!
 * \param list	list
 * \param iter	iterator pointing to the item, that should be moved
 */
void LIST_MoveItemBottom(TList list, TListIter iter)
{
	ASSERT(list && iter)

	// Move the item if not already at the bottom
	if (iter->Next != &list->Tail) {
		iter->Prev->Next = iter->Next;
		iter->Next->Prev = iter->Prev;

		iter->Next = &list->Tail;
		iter->Prev = list->Tail.Prev;
		list->Tail.Prev = iter;
		iter->Prev->Next = iter;
	}
}

//! Moves the element pointed to by iterator to the front of the list
/*!
 * \param list	list
 * \param iter	iterator
 */
void LIST_SetIterFirst(TList list, TListIter iter)
{
	TListIter old_top;
	TListIter tmp;

	ASSERT(list && iter)

	old_top = list->Tail.Next;
	tmp = old_top->Next;

	old_top->Next = iter->Next;
	old_top->Prev = iter->Prev;

	list->Tail.Next = iter;
	iter->Prev = &list->Tail;

	iter->Next = tmp;
}

//! Moves the element pointed to by iterator to the end of the list
/*! TODO: not finished yet
 * \param list	list
 * \param iter	iterator
 */
void LIST_SetIterLast(TList list, TListIter iter)
{
	ASSERT(list && iter)
}

/*
 * Find an element in the list and return an iterator pointing to it.
 * Note that this is very slow because it traverses the entire list
 * searching for an element.
 */
TListIter LIST_FindItem(TList list, void *data, int32_t offset, int32_t size)
{
	ASSERT(list && data)

	return LIST_FindItemRange(LIST_GetFirstItem(list), LIST_GetLastItem(list), data, offset, size);
}

/*
 * Find an element in a range of elements (excluding last) and return
 * an iterator pointing to it.  Note that this is a very slow operation.
 *
 * Return NULL if the element hasn't been found
 */
TListIter LIST_FindItemRange(TListIter first, TListIter last, void *data, int32_t offset, int32_t size)
{
	TListIter pos = first;
	bool not_found = true;

	ASSERT(first && last && data)

	while (pos != last && not_found) {
		ASSERT(pos->Data)

		not_found = bcmp(pos->Data + offset, data, size);
		pos = pos->Next;
	}

	if (not_found) {
		return NULL;
	}
	else {
		return pos->Prev;
	}
}

/*
 * Remove all element from the list which are equal to Data.
 * Note that this is very slow because it traverses the entire list.
 * The return Value is the number of successful removals.
 */
//int32_t		List_remove(TList list, void *data)
//{
//    TListIter		pos = List_begin(list);
//    TListIter		end = List_end(list);
//    int32_t			count = 0;
//
//    while (pos != end) {
//	pos = List_find_range(pos, end, data);
//	if (pos != end) {
//	    pos = List_erase(list, pos);
//	    count++;
//	}
//    }
//
//    return count;
//}

/* return the number of elements in the list. */
int32_t LIST_GetItemCount(TList list)
{
	ASSERT(list)

	return list->Size;
}

/* advance list iterator one position and return new position. */
TListIter LIST_ForwardIter(TListIter *pos)
{
	ASSERT(pos)

	(*pos) = (*pos)->Next;
	return (*pos);
}

/* move list iterator one position backwards and return new position. */
TListIter LIST_BackIter(TListIter *pos)
{
	ASSERT(pos && *pos)

	(*pos) = (*pos)->Prev;
	return (*pos);
}

/* return Data at list position. */
void* LIST_GetItemData(TListIter pos)
{
	ASSERT(pos)

	return pos->Data;
}

int32_t LIST_GetListCount(void)
{
	return LIST_NumLists;
}

int32_t LIST_GetNodeCount(void)
{
	return LIST_NumNodes;
}

void LIST_GetItemsToBuffer(TList list, char *buf, int32_t item_size)
{
	TListIter l;
	TListIter l_bottom;
	char *ptr = buf;

	ASSERT(list && buf)

	l = list->Tail.Next;
	l_bottom = &list->Tail;

	while (l != l_bottom) {
		memcpy(ptr, l->Data, item_size);
		ptr += item_size;
		l = LIST_ForwardIter(&l);
	}
}

void LIST_AddItemsFromBuffer(TList list, char *buf, int32_t item_size, int32_t num_items)
{
	char *ptr;
	char *ptr2;
	int32_t i;

	ASSERT(list && buf)

	ptr2 = buf;

	for (i = 0; i < num_items; i++) {
		ptr = malloc(item_size);
		memcpy(ptr, ptr2, item_size);
		LIST_AddItemBottom(list, ptr);
		ptr2 += item_size;
	}
}
