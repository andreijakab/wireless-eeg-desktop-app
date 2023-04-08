/**
 * \file		linkedlist.h
 * \since		28.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 *
 * \brief		???
 *
 * $Id: linkedlist.h 78 2013-02-21 17:23:21Z jakab $
 */

# ifndef __LINKEDLIST_H__
# define __LINKEDLIST_H__

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
#ifdef _DEBUG
//#define _DEBUG_LL
#endif
//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
/**
 * Structure of a linked list node.
 */
typedef struct _linkedlist_item
{	void *						value;	///< pointer to data buffer
	struct _linkedlist_item *	prev;	///< pointer to previous node in the linked list
	struct _linkedlist_item *	next;	///< pointer to next node in the linked list
} linkedlist_item;

/**
 * Structure representing the doubly-linked list.
 */
typedef struct
{
	int					count;			///< amount of nodes in the linked list
	linkedlist_item *	head;			///< pointer to head node
	linkedlist_item *	tail;			///< pointer to tail node
	HANDLE				hMutex;			///< handle to mutex that guards the access to the linked list
	char				ID;				///< character used to ID the linked list (used only for debugging purposes)
} linkedlist;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
/**
 * \brief Creates the managing structure of a doubly-linked list.
 *
 * \param[in]	cID		character used to ID the linked list (can be set to 0 since this is used only for debugging purposes)
 *
 * \return Pointer to the linked list managing structure if function executes successfully, NULL otherwise.
 */
linkedlist * linkedlist_create(char tID);

/**
 * \brief Destroy linked list by releasing all memory allocated to it (including its nodes).
 *
 * \param[in]	l	pointer to linked list managing structure (created by linkedlist_create())
 */
void linkedlist_free(linkedlist * l);

/**
 * \brief Adds node to the tail of the linked list.
 *
 * \param[in]	l		pointer to linked list managing structure
 * \param[in]	ptr		pointer to data to be associated with the element
 *
 * \return Pointer to the new node if function executes successfully, NULL otherwise.
 */
linkedlist_item * linkedlist_add_element(linkedlist * l, void * ptr);

/**
 * \brief Get reference to the head element of the the linked list.
 *
 * \param[in]	l		pointer to manging structure of linked list from where head node is to be retrieved
 *
 * \return Pointer of linkedlist_item type to the head node of the list, or NULL if list is invalid or empty.
 */
linkedlist_item * linkedlist_get_first(linkedlist * l);

/**
 * \brief Remove a given node from the linked list.
 *
 * \param[in]	l		pointer to manging structure of linked list from where node is to be removed
 * \param[in]	li		pointer to node to be removed from linked list
 *
 * \return TRUE if node was found and removed, FALSE otherwise.
 */
BOOL linkedlist_remove_element(linkedlist * l, linkedlist_item * li);

/**
 * \brief Get reference to the head element of the the linked list.
 *
 * \param[in]	l		pointer to manging structure of linked list from where head node is to be retrieved
 *
 * \return Pointer of value stored in the lists's head node, or NULL if list is invalid or empty.
 */
void * linkedlist_remove_headelement(linkedlist * l);

#endif