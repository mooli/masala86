// -*- mode: c++ -*-
/**
   \brief List processing (implementation)
   \file
*/

#include "exec/list.hpp"
using namespace exec;

/**
   \defgroup exec_list Lists and (Min)Nodes: simple doubly-linked lists

   MinList and MinNode work together to produce a simple doubly-linked list.
   These are much more low-level than std::list<> in that objects in a MinList
   must subclass MinNode (which contains the link pointers) and are linked
   together in place without copying or dynamic allocation. The main use of
   this is for implementing the dynamic memory allocator itself.

   A MinNode is simply a pair of pointers to the next and previous nodes in the
   list.

   A Node is a MinNode that also includes a name and priority. These can be
   used to form dictionaries and priority queues, although because they're
   implemented in terms of a linked list, have O(N) performance and thus
   shouldn't be used where in performance-critical areas or where the list may
   become large.

   A MinList contains a pair of semi-overlapping MinNode%s that are effectively
   the one-before-head and one-after-tail node. This simplifies list handling
   because there is no longer a need to special-case handling of nodes at the
   start and end of the list.

   MinLists can be iterated over in much the same manner as STL containers
   thus:

   \code
   class Foo : public MinNode {
       void do_something(void);
   };

   class FooList : MinList<Foo> {
       void do_all(void) {
           for(FooList::iterator foo = foolist.begin(); foo != foolist.end(); ++foo) {
               foo->do_something();
           }
       }
   };

   Foo foo;
   FooList foolist;
   foolist.push(&foo);
   foolist.do_all();

   \endcode

 */
