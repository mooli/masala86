// -*- mode: c++ -*-
/**
   \brief List processing (headers)
   \file
*/

#ifndef EXEC_LIST_HPP
#define EXEC_LIST_HPP

#include <assert.h>
#include <stddef.h>             // for NULL
#include "exec/types.hpp"

/** \addtogroup exec_list
    @{ */

/** \brief a minimal doubly-linked list node

 */
class exec::MinNode {
    template <class T> friend class MinList;
    MinNode *next;              //!< pointer to next MinNode
    MinNode *prev;              //!< pointer to previous MinNode

    MinNode(const MinNode &) = delete;            //!< **deleted**
    MinNode &operator=(const MinNode &) = delete; //!< **deleted** \returns nothing

    /** \brief tests whether this is the end-of-list marker
        \returns true if this is the end-of-list marker */
    bool iseolm(void) const { return next == NULL; }
    /** \brief tests whether this is the start-of-list marker
        \returns true if this is the start-of-list marker */
    bool issolm(void) const { return prev == NULL; }
    /** \brief removes this MinNode from the MinList it's in
        \returns the removed MinNode */
    MinNode *remove(void) {
        assert(prev && next);   // check node is in a list
        prev->next = next;
        next->prev = prev;
#ifndef NDEBUG
        prev = next = NULL;
#endif
        return this;
    }
public:
#ifdef NDEBUG
    //! \brief empty default constructor: does not initialise MinNode
    MinNode(void) : next(), prev() {}
#else
    //! \brief empty default constructor: NULLs link fields
    MinNode(void) : next(NULL), prev(NULL) {}
#endif
    /** \brief inserts this MinNode after another MinNode
        \param existing the MinNode to insert after */
    void insert_after(MinNode *existing) __attribute__((nonnull)) {
        assert(!prev && !next);           // check this node is not in a list
        assert(existing);                 // check for non-null existing node
        assert(existing->prev && existing->next); // check for double-insert
        prev = existing;
        next = existing->next;
        next = existing->next;
        next->prev = existing->next = this;
    }
    /** \brief inserts this MinNode before another MinNode
        \param existing the MinNode to insert before */
    void insert_before(MinNode *existing) __attribute__((nonnull)) {
        assert(!prev && !next);           // check this node is not in a list
        assert(existing);                 // check for non-null existing node
        assert(existing->prev && existing->next); // check for double-insert
        next = existing;
        prev = existing->prev;
        prev->next = existing->prev = this;
    }
};

/** \brief a doubly-linked list node

 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Node : public MinNode {
#pragma GCC diagnostic pop
public:
    const char *name;           //!< name of this node
    int priority;               //!< priority of this node
    Node(const char *name_, int priority_) : name(name_), priority(priority_) {}
};

/** \brief a doubly-linked list

 */
template <class T> class exec::MinList {
    MinNode *head;              //!< start-of-list marker node
    const void *tail;           //!< end-of-list marker node
    MinNode *tail_prev;         //!< prev field of end-of-list marker node
    MinList(const MinList &) = delete; //!< **deleted**
    MinList &operator=(const MinList &) = delete; //!< **deleted** \returns nothing
    /** \brief get start-of-list node \returns the address of the start-of-list marker node */

    MinNode *solm(void) { return reinterpret_cast<MinNode *>(&head); }
    /** \brief get start-of-list node \returns the address of the start-of-list marker node */
    const MinNode *solm(void) const { return reinterpret_cast<const MinNode *>(&head); }
    /** \brief get end-of-list node \returns the address of the end-of-list marker node */
    MinNode *eolm(void) { return reinterpret_cast<MinNode *>(&tail); }
    /** \brief get end-of-list node \returns the address of the end-of-list marker node */
    const MinNode *eolm(void) const { return reinterpret_cast<const MinNode *>(&tail); }
public:
    class iterator;             //!< type of an iterator over a MinList
    class const_iterator;       //!< type of an iterator over a const MinList
    //! constructs a new empty MinList
    MinList(void)
        : head(eolm()),
          tail(NULL),
          tail_prev(solm())
    {}
    //! test for emptiness \returns true if the MinList is empty
    bool isempty(void) const { return head == eolm(); }
    /** \brief adds a node to the start of the MinList
        \param that the node to insert */
    void unshift(T *that) __attribute__((nonnull)) {
        assert(that);                       // check for insert of NULL
        assert(!that->prev && !that->next); // check for double-insert
        that->next = head;
        that->prev = solm();
        head->prev = that;
        head = that;
    }
    /** \brief adds a node to the end of the MinList
        \param that the node to insert */
    void push(T *that) __attribute__((nonnull)) {
        assert(that);                       // check for insert of NULL
        assert(!that->prev && !that->next); // check for double-insert
        that->prev = tail_prev;
        that->next = eolm();
        tail_prev->next = that;
        tail_prev = that;
    }
    /** \brief removes a node from the start of the MinList
        \see first() which does not remove the node
        \returns the removed node, or NULL if the MinList was empty */
    T *shift(void) { return isempty() ? NULL : static_cast<T *>(head->remove()); }
    /** \brief removes a node from the end of the MinList
        \see last() which does not remove the node
        \returns the removed node, or NULL if the MinList was empty */
    T *pop(void) { return isempty() ? NULL : static_cast<T *>(tail_prev->remove()); }
    /** \brief gets the node at the start of the MinList
        \see shift() which also removes the node
        \returns the first node, or NULL if the MinList was empty */
    T *first(void) { return isempty() ? NULL : static_cast<T *>(head); }
    /** \brief gets the node at the end of the MinList
        \see pop() which also removes the node
        \returns the last node, or NULL if the MinList was empty */
    T *last(void) { return isempty() ? NULL : static_cast<T *>(tail); }
    /** \brief removes this node from the MinList it's in
        \param minnode the node to return
        \returns the removed node (i.e. minnode itself) */
    static T *remove(T *minnode) __attribute__((nonnull)) {
        assert(minnode);
        return static_cast<T *>(minnode->remove());
    }
    /** \brief inserts a node after another node
        \param existing the existing node that we will insert after
        \param inserted the new node that will be inserted after
    */
    static void insert_after(T *existing, T *inserted) __attribute__((nonnull)) {
        inserted->insert_after(existing);
    }
    /** \brief inserts a node before another node
        \param existing the existing node that we will insert before
        \param inserted the new node that will be inserted before
    */
    static void insert_before(T *existing, T *inserted) __attribute__((nonnull)) {
        inserted->insert_before(existing);
    }
    /** \brief inserts a node in priority order
        \param node the node to insert
    */
    void enqueue(T *node) {
        for(iterator i = begin(); i != end(); ++i) {
            if(i->priority < node->priority) {
                insert_before(i, node);
                return;
            }
        }
        push(node);
    }
    /** \brief finds a node by name
        \param name the name of the node to find
        \param start the node before the node to search from, or NULL to search the whole list
        \returns the Node found, or NULL if not found
        \todo we might need a const version
        \todo not yet tested in anger
    */
    Node *find_name(const char *name, Node *start) __attribute__((nonnull(2))) {
        iterator i = iterator(start ? start : solm()),
            e = this->end();
        while(++i != e)
            if(i->name && !strcmp(name, i->name))
                return *i;
        return NULL;
    }

    iterator begin(void);
    iterator end(void);
    const_iterator begin(void) const;
    const_iterator end(void) const;
};

/** \brief an iterator over a MinList

 */
template <class T> class exec::MinList<T>::iterator {
    T *ptr;                     //!< node the iterator currently points at
public:
    /** \brief constructs an iterator pointing to a given node
        \param p pointer to the node */
    explicit iterator(T *p) __attribute__((nonnull(2)))
        : ptr(p) {}
    /** \brief dereference iterator
        \returns reference to the node that the iterator points at */
    T &operator*(void) const { return *ptr; }
    /** \brief dereference iterator
        \returns pointer to the node that the iterator points at */
    T *operator->(void) const { return ptr; }
    /** \brief coerce back to node
        \returns pointer to the node that the iterator points at */
    operator T *(void) const { return ptr; };
    /** \brief advances the iterator to the next node
        \returns an iterator pointing the next node */
    iterator &operator++(void) {
        ptr = static_cast<T *>(ptr->next);
        return *this;
    }
    /** \brief advances the iterator to the next node
        \returns an iterator pointing to the current node */
    iterator operator++(int) {
        iterator ret(ptr);
        this->operator++();
        return ret;
    }
    /** \brief test for equality
        \param that the iterator to compare against
        \returns whether the two iterators point to the same node */
    bool operator==(const iterator &that) const { return ptr == that.ptr; }
    /** \brief test for inequality
        \param that the iterator to compare against
        \returns whether the two iterators point to different node%s */
    bool operator!=(const iterator &that) const { return ptr != that.ptr; }
};

/** \brief an iterator over a const MinList

 */
template <class T> class exec::MinList<T>::const_iterator {
    const T *ptr;            //!< node the iterator currently points at
public:
    /** \brief constructs an iterator pointing to a given node
        \param p pointer to the node */
    explicit const_iterator(const T *p) __attribute__((nonnull(2)))
        : ptr(p) {}
    /** \brief dereference iterator
        \returns the node * that the iterator points at */
    const T &operator*(void) const { return *ptr; }
    /** \brief dereference iterator
        \returns the node * that the iterator points at */
    const T *operator->(void) const { return ptr; }
    /** \brief advances the iterator to the next node
        \returns an iterator pointing the next node */
    const_iterator &operator++(void) {
        ptr = static_cast<T *>(ptr->next);
        return *this;
    }
    /** \brief coerce back to node
        \returns pointer to the node that the iterator points at */
    operator const T *(void) const { return ptr; };
    /** \brief advances the iterator to the next node
        \returns an iterator pointing to the current node */
    const_iterator operator++(int) {
        const_iterator ret(ptr);
        this->operator++();
        return ret;
    }
    /** \brief test for equality
        \param that the iterator to compare against
        \returns whether the two iterators point to the same node */
    bool operator==(const const_iterator &that) const { return ptr == that.ptr; }
    /** \brief test for inequality
        \param that the iterator to compare against
        \returns whether the two iterators point to different node%s */
    bool operator!=(const const_iterator &that) const { return ptr != that.ptr; }
};

/** @} */

/** \brief gets an iterator pointing at the first node
    \returns an iterator pointing at the first node */
template <class T> inline typename exec::MinList<T>::iterator exec::MinList<T>::begin(void)
{
    return iterator(static_cast<T *>(head));
}
/** \brief gets an iterator pointing at the end-of-minlist marker
    \returns an iterator pointing at the end-of-minlist marker */
template <class T> inline typename exec::MinList<T>::iterator exec::MinList<T>::end(void)
{
    return iterator(static_cast<T *>(eolm()));
}
/** \brief gets a const_iterator pointing at the first node
    \returns a const_iterator pointing at the first node */
template <class T> inline typename exec::MinList<T>::const_iterator exec::MinList<T>::begin(void) const
{
    return const_iterator(static_cast<const T *>(head));
}
/** \brief gets a const_iterator pointing at the end-of-minlist marker
    \returns a const_iterator pointing at the end-of-minlist marker */
template <class T> inline typename exec::MinList<T>::const_iterator exec::MinList<T>::end(void) const
{
    return const_iterator(static_cast<const T *>(eolm()));
}

#endif
