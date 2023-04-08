/**
 * \file		linkedlist.cpp
 * \since		28.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 * \version		1.0.0
 *
 * \brief		Functions for creating and managing a doubly linked list.
 *
 * $Id: linkedlist.cpp 78 2013-02-21 17:23:21Z jakab $
 */

//---------------------------------------------------------------------------
//   					  Windows-related definitions
//---------------------------------------------------------------------------
// this macro prevents windows.h from including winsock.h for version 1.1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// library requires at least Windows XP SP2
#define WINVER			0x0502
#define _WIN32_WINNT	0x0502
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//---------------------------------------------------------------------------
//									Libraries
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//   							Includes
//------------------------------------------------------------ ---------------
// Windows headers
#include <Windows.h>

// CRT headers
#include <stdlib.h>
#include <stdio.h>

// program headers
#include "globals.h"
#include "applog.h"
#include "linkedlist.h"

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//							Global variables
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//							Internally-accessible functions
//---------------------------------------------------------------------------

void linkedlist_testModule(void)
{
	char				cAlphabet[26] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
	char				strDebugBuffer[256];
	linkedlist *		ll;
	unsigned int		i;
	void *				item;

	// create linked list
	ll = linkedlist_create(0);
	if(ll == NULL)
	{
		sprintf_s(strDebugBuffer, _countof(strDebugBuffer), "linkedlist_testModule(): Unable to create test linked list.\n");
		OutputDebugStringA(strDebugBuffer);
		return;
	}

	// add test elements to linked list
	for(i = 0; i < _countof(cAlphabet); i++)
	{
		if(linkedlist_add_element(ll, cAlphabet + i) == NULL)
		{
			sprintf_s(strDebugBuffer, _countof(strDebugBuffer), "linkedlist_testModule(): Unable to add element %d ('%c') to linked list.\n", i, cAlphabet[i]);
			OutputDebugStringA(strDebugBuffer);
			linkedlist_free(ll);
			return;
		}
	}

	// output linked list elements
	while(ll->count > 0)
	{
		item = linkedlist_remove_headelement(ll);
		if(item != NULL)
		{
			sprintf_s(strDebugBuffer, _countof(strDebugBuffer), "linkedlist_testModule(): Retrieved element '%c' from linked list.\n", *((char *) item));
			OutputDebugStringA(strDebugBuffer);
		}
		else
		{
			sprintf_s(strDebugBuffer, _countof(strDebugBuffer), "linkedlist_testModule(): Error occure while retrieving element from non-empty linked list.\n");
			OutputDebugStringA(strDebugBuffer);
			linkedlist_free(ll);
			return;
		}
	}

	// destroy linked list
	linkedlist_free(ll);
}

//---------------------------------------------------------------------------
//							Globally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief Creates the managing structure of a doubly-linked list.
 *
 * \param[in]	cID		character used to ID the linked list (can be set to 0 since this is used only for debugging purposes)
 *
 * \return Pointer to the linked list managing structure if function executes successfully, NULL otherwise.
 */
linkedlist * linkedlist_create(char cID)
{
	linkedlist * l;
	
	// allocate memory for managing structure and initialize its variables
	l = (linkedlist *) malloc(sizeof(linkedlist));
	if(l != NULL)
	{
		l->ID = cID;
		l->count = 0;
		l->head = NULL;
		l->tail = NULL;
		l->hMutex = CreateMutex(NULL,			// no security descriptors (handle cannot be inherited by child processes)
								FALSE,			// caller doesn't obtain inital ownership of mutex object
								NULL);			// unnamed mutex object
		if(l->hMutex == NULL)
		{
			free(l);
			l = NULL;
			applog_logevent(SoftwareError, TEXT("LinkedList"), TEXT("linkedlist_create() - Mutex creation failed. (GetLastError() #)"), GetLastError(), TRUE);
		}
	}
	else
		applog_logevent(SoftwareError, TEXT("LinkedList"), TEXT("linkedlist_create() - Unable to allocate memory for the managing structure of the linked list. (errno #)"), errno, TRUE);

	return l;
}

/**
 * \brief Destroy linked list by releasing all memory allocated to it (including its nodes).
 *
 * \param[in]	l	pointer to linked list managing structure
 */
void linkedlist_free(linkedlist * l)
{
	linkedlist_item * li, *tmp;

	// wait for sole access to linked list
	WaitForSingleObject(l->hMutex, INFINITE);
	
	if (l != NULL)
	{
		li = l->head;

		// travel from head to tail of linked list and clear all nodes that are encountered
		while (li != NULL)
		{
			tmp = li->next;
			
			free(li->value);
			free(li);
			
			li = tmp;
		}
	}
	
	// release and free mutex by closing its handle
	ReleaseMutex(l->hMutex);
	CloseHandle(l->hMutex);

	// free memory allocated to linked list managing structure
	free(l);
}

/**
 * \brief Adds node to the tail of the linked list.
 *
 * \param[in]	l		pointer to linked list managing structure
 * \param[in]	ptr		pointer to data to be associated with the element
 *
 * \return Pointer to the new node if function executes successfully, NULL otherwise.
 */
linkedlist_item * linkedlist_add_element(linkedlist * l, void * ptr)
{
	linkedlist_item *	li;
#ifdef _DEBUG_LL
	char				strDebugBuffer[256];
#endif

	// wait for sole access to linked list
	WaitForSingleObject(l->hMutex, INFINITE);

	// allocate memory for new node and initilize its variables
	li = (linkedlist_item *) malloc(sizeof(linkedlist_item));
	if(li != NULL)
	{
		// store data pointer in element
		li->value = ptr;

		// set node's into linked list
		li->next = NULL;
		li->prev = l->tail;

		// update linked list managing structure
		if (l->tail == NULL)
		{
			// since there is no tail node, list is currently empty and new node is head node
			l->head = l->tail = li;
		}
		else
		{
			// connect current tail node to new node
			l->tail->next = li;

			// set new node as tail node
			l->tail = li;
		}

		l->count++;

#ifdef _DEBUG_LL
		sprintf_s(strDebugBuffer, _countof(strDebugBuffer), "linkedlist_add_element() to list '%c' - new count: %d\n", l->ID, l->count);
		OutputDebugStringA(strDebugBuffer);
#endif
	}
	else
		applog_logevent(SoftwareError, TEXT("LinkedList"), TEXT("list_add_element() - Unable to allocate memory for the node. (errno #)"), errno, TRUE);

	// release mutex
	ReleaseMutex(l->hMutex);

	return li;
}

/**
 * \brief Get reference to the head element of the the linked list.
 *
 * \param[in]	l		pointer to manging structure of linked list from where head node is to be retrieved
 *
 * \return Pointer of value stored in the lists's head node, or NULL if list is invalid or empty.
 */
void * linkedlist_remove_headelement(linkedlist * l)
{
	linkedlist_item * pli;
	void * value = NULL;
	
	// wait for sole access to linked list
	WaitForSingleObject(l->hMutex, INFINITE);

	// check if argument is valid list
	if (l != NULL)
	{
		// check if list has at list one element
		if(l->count > 0)
		{
			// retrieve head element of linked list
			pli = l->head;
			value = pli->value;

			// remove head element from linked list
			linkedlist_remove_element(l, pli);
		}
	}

	// release mutex
	ReleaseMutex(l->hMutex);

	return value;
}

/**
 * \brief Remove a given node from the linked list.
 *
 * \param[in]	l		pointer to manging structure of linked list from where node is to be removed
 * \param[in]	li		pointer to node to be removed from linked list
 *
 * \return TRUE if node was found and removed, FALSE otherwise.
 */
BOOL linkedlist_remove_element(linkedlist * l, linkedlist_item * li)
{
	BOOL				blnResult = FALSE;
	linkedlist_item *	tmp = l->head;
#ifdef _DEBUG_LL
	char				strDebugBuffer[256];
#endif

	// wait for sole access to tmpnked tmpst
	WaitForSingleObject(l->hMutex, INFINITE);
	
	// search linked list for node
	while (tmp != NULL)
	{
		if (tmp == li)
		{
			// remove node from linked list
			if (tmp->prev == NULL)
			{
				l->head = tmp->next;
			}
			else
			{
				tmp->prev->next = tmp->next;
			}

			if (tmp->next == NULL)
			{
				l->tail = tmp->prev;
			}
			else
			{
				tmp->next->prev = tmp->prev;
			}

			l->count--;

#ifdef _DEBUG_LL
		sprintf_s(strDebugBuffer, _countof(strDebugBuffer), "linkedlist_remove_element() to list '%c' - new count: %d\n", l->ID, l->count);
		OutputDebugStringA(strDebugBuffer);
#endif

			// free memory allocated to node
			free(tmp);
			
			// signal that removal was successfull and break out of loop
			blnResult = TRUE;
			break;
		}

		tmp = tmp->next;
	}
	
	// release mutex
	ReleaseMutex(l->hMutex);
	
	return blnResult;
}

/*
void list_each_element(list *l, int (*func)(list_item *))
{
	list_item *li;

	pthread_mutex_lock(&(l->mutex));

	li = l->head;

	while (li != NULL)
	{

		if (func(li) == 1)
		{
			break;
		}

		li = li->next;
	}

	pthread_mutex_unlock(&(l->mutex));
}*/