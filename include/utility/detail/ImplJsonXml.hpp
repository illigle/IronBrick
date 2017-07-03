/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an "as is" basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#ifndef _IRK_PRIVATE_IMPLJSONXML_H_
#define _IRK_PRIVATE_IMPLJSONXML_H_

#include <stdint.h>
#include <assert.h>

// define common data struct for JSON and XML parser

namespace irk { namespace detail {

// simple intrusive single list, Node class must have member m_pNext
template<class NodeTy>
struct SList
{
    SList() : m_firstNode(nullptr), m_lastNode(nullptr) {}
    NodeTy* m_firstNode;    // the first element in the single list
    NodeTy* m_lastNode;     // the last element in the single list

    // add new node to the tail
    void push_back( NodeTy* newNode )
    {
        if( !m_firstNode )          // the first node
            m_firstNode = newNode;
        else
            m_lastNode->m_pNext = newNode;  // add to the tail

        newNode->m_pNext = nullptr;
        m_lastNode = newNode;
    }

    // add new node to the head
    void push_front( NodeTy* newNode )
    {
        newNode->m_pNext = m_firstNode;
        m_firstNode = newNode;

        if( !m_lastNode )           // the first node
            m_lastNode = newNode;
    }

    // find the desired node, return nullptr if failed
    NodeTy* find( int idx ) const;
    template<class Pred>
    NodeTy* find( Pred ) const; // with an general pred functor

    // remove node from list
    // return the removed node if succeeded, return nullptr if failed
    NodeTy* remove( int idx );
    NodeTy* remove( NodeTy* victim );
    template<class Pred>
    NodeTy* remove( Pred );     // with an general pred functor

    // clear list
    void clear()
    {
        m_firstNode = nullptr;
        m_lastNode = nullptr;
    }

private:
    void unlink_node( NodeTy* prevNode, NodeTy* currNode )
    {
        assert( currNode );

        if( !currNode->m_pNext )    // remove the last node
            m_lastNode = prevNode;

        if( !prevNode )             // remove the first node
            m_firstNode = currNode->m_pNext;
        else
            prevNode->m_pNext = currNode->m_pNext;
    }
};

// find the desired node, return nullptr if failed
template<class NodeTy>
NodeTy* SList<NodeTy>::find( int idx ) const
{
    if( idx < 0 )
        return nullptr;

    NodeTy* currNode = m_firstNode;
    while( currNode && (idx-- > 0) )
        currNode = currNode->m_pNext;
    return currNode;
}

// find the desired node, return nullptr if failed
template<class NodeTy>
template<class Pred>
NodeTy* SList<NodeTy>::find( Pred pred ) const
{
    NodeTy* currNode = m_firstNode;
    while( currNode && !pred(currNode) )
        currNode = currNode->m_pNext;
    return currNode;
}

// remove node from list
template<class NodeTy>
NodeTy* SList<NodeTy>::remove( int idx )
{
    if( idx < 0 )
        return nullptr;

    // find the victim
    NodeTy* prevNode = nullptr;
    NodeTy* currNode = m_firstNode;
    while( currNode && (idx-- > 0) )
    {
        prevNode = currNode;
        currNode = currNode->m_pNext;
    }

    if( currNode )      // found
    {
        this->unlink_node( prevNode, currNode );
        return currNode;
    }
    return nullptr;
}

// remove node from list
template<class NodeTy>
NodeTy* SList<NodeTy>::remove( NodeTy* victim )
{
    // find the victim
    NodeTy* prevNode = nullptr;
    NodeTy* currNode = m_firstNode;
    while( currNode && (currNode != victim) )
    {
        prevNode = currNode;
        currNode = currNode->m_pNext;
    }

    if( currNode )      // found
    {
        this->unlink_node( prevNode, currNode );
        return currNode;
    }
    return nullptr;
}

// remove node from list
template<class NodeTy>
template<class Pred>
NodeTy* SList<NodeTy>::remove( Pred pred )
{
    // find the victim
    NodeTy* prevNode = nullptr;
    NodeTy* currNode = m_firstNode;
    while( currNode && !pred(currNode) )
    {
        prevNode = currNode;
        currNode = currNode->m_pNext;
    }

    if( currNode )      // found
    {
        this->unlink_node( prevNode, currNode );
        return currNode;
    }
    return nullptr;
}

// Iterator template used to traverse JSON Array, JSON Object, XML elements, XML attributes
template<class Ty>
class JXIterator
{
public:
    typedef Ty value_type;
    explicit JXIterator( Ty* val ) : m_pcur(val) {}
    bool operator==( const JXIterator& other ) const  { return m_pcur == other.m_pcur; }
    bool operator!=( const JXIterator& other ) const  { return m_pcur != other.m_pcur; }

    JXIterator& operator++()
    {
        if( m_pcur ) { m_pcur = m_pcur->next_sibling(); }
        return *this;
    }
    Ty* operator->() const { return m_pcur; }
    Ty& operator*() const  { return *m_pcur; }
private:
    Ty* m_pcur;
};

}}      // end of namespace irk::detail

#endif
