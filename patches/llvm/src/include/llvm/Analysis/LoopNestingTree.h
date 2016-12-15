/*
 * Loop-nesting tree for a loop.  The root of a loop-nesting tree is the loop
 * header of the outermost loop.  The children of any given node (including the
 * root) are the basic blocks contained within the loop and the loop headers of
 * nested loops.
 *
 * Note: we assume that the control-flow graphs are reducible
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/23/2016
 */

#ifndef _LOOP_NESTING_TREE_H
#define _LOOP_NESTING_TREE_H

#include <list>
#include <vector>
#include <queue>
#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"

class LoopNestingTree {
private:
  /*
   * Tree node object.
   */
  class Node {
  public:
    /**
     * Construct a node for a basic block.
     * @param _bb a basic block
     * @param _parent the parent of this node, i.e. the loop header of the
     *                containing loop
     * @param _isLoopHeader is the basic block a loop header?
     */
    Node(const llvm::BasicBlock *_bb, const Node *_parent, bool _isLoopHeader)
      : bb(_bb), parent(_parent), isLoopHeader(_isLoopHeader) {}

    /**
     * Add a child to the node.
     * @param child a child to add to the node
     */
    void addChild(Node *child) { children.push_back(child); }

    const llvm::BasicBlock *bb; /* Basic block encapsulated by the node. */
    const Node *parent; /* Parent node, i.e. header of containing loop. */
    std::list<Node *> children; /* Regular child nodes in the tree. */
    bool isLoopHeader; /* Is the basic block a loop header? */
  };

  unsigned _size; /* Number of nodes (i.e., basic blocks) in the tree. */
  unsigned _depth; /* Number of nested loops in the tree. */
  Node *_root; /* Root of the tree, i.e. loop header of outermost loop. */

  /**
   * Print a node & its children.  Recurses into nested loops.
   * @param O an output stream on which to print the tree
   * @param node a node to print
   * @param depth the current depth
   */
  void print(llvm::raw_ostream &O, Node *node, unsigned depth) const;

  /**
   * Delete the node's children & the node itself.  Recurses into nested loops.
   * @param node the node being deleted
   */
  void deleteRecursive(Node *node);

public:
  /**
   * Construct a loop-nesting tree from a strongly-connected component of the
   * control-flow graph.
   * @param SCC a strongly-connected component of the control-flow graph
   * @param LI analysis from the loop info pass
   */
  LoopNestingTree(const std::vector<llvm::BasicBlock *> &SCC,
                  const llvm::LoopInfo &LI);

  /**
   * Destroy a loop-nesting tree.
   */
  ~LoopNestingTree() { deleteRecursive(this->_root); }

  /**
   * Return the size of the loop-nesting tree, that is the number of nodes in
   * the loop (and all nested loops).
   * @return the number of nodes in the tree
   */
  unsigned size() const { return this->_size; }

  /**
   * Return the depth of the loop-nesting tree, that is the number of nested
   * loops.  A value of one indicates that there are no nested loops.
   * @return the number of nested loops in the tree
   */
  unsigned depth() const { return this->_depth; }

  /**
   * Print the tree.
   * @param O the output stream on which to print the tree
   */
  void print(llvm::raw_ostream &O) const { print(O, this->_root, 0); }

  /*
   * Loop-node iterator object.  Delivers loop nodes in breadth-first order.
   */
  class loop_iterator {
  public:
    typedef loop_iterator self_type;
    typedef const llvm::BasicBlock *value_type;
    typedef value_type& reference;
    typedef value_type* pointer;
    typedef std::forward_iterator_tag iterator_category;

    self_type operator++(void);
    self_type operator++(int junk);
    reference operator*(void) { return cur->bb; }
    pointer operator->(void) { return &cur->bb; }
    bool operator==(const self_type& rhs) { return cur == rhs.cur; }
    bool operator!=(const self_type& rhs) { return cur != rhs.cur; }

    friend class LoopNestingTree;
    friend class child_iterator;
  private:
    Node *cur;
    std::queue<Node *> remaining;

    loop_iterator(Node *start) : cur(start) { addLoopHeaders(); }
    void addLoopHeaders(void);
  };

  /*
   * Child iterator object.  Traverses children of tree nodes.
   */
  class child_iterator {
  public:
    typedef child_iterator self_type;
    typedef const llvm::BasicBlock *value_type;
    typedef value_type& reference;
    typedef value_type* pointer;
    typedef std::forward_iterator_tag iterator_category;
    enum location { BEGIN, END };

    self_type operator++(void)
      { self_type me = *this; it.operator++(); return me; }
    self_type operator++(int junk) { it.operator++(junk); return *this; }
    reference operator*(void) { return (*it)->bb; }
    pointer operator->(void) { return &(*it)->bb; }
    bool operator==(const self_type& rhs) { return it == rhs.it; }
    bool operator!=(const self_type& rhs) { return it != rhs.it; }

    friend class LoopNestingTree;
  private:
    std::list<Node *>::const_iterator it;

    child_iterator(loop_iterator &parent, enum location loc);
  };

  /**
   * Return an iterator for traversing all loop nodes (i.e., loop header basic
   * blocks) in the tree.  Delivers nodes in a breadth-first ordering.
   * @return an iterator to traverse the loop nodes in the tree
   */
  loop_iterator loop_begin() const { loop_iterator it(_root); return it; };

  /**
   * Return an iterator marking the end of the loop nodes in the tree.
   * @return an iterator marking the end of the traversal
   */
  loop_iterator loop_end() const { loop_iterator it(nullptr); return it; };

  /**
   * Return an iterator for traversing the children of a loop node.
   * @param an iterator associated with a loop node
   * @return an iterator to traverse the children of a loop node
   */
  child_iterator children_begin(loop_iterator &parent) const
    { child_iterator it(parent, child_iterator::BEGIN); return it; }

  /**
   * Return an iterator marking the end of the children of a loop node.
   * @param an iterator associated with a loop node
   * @return an iterator marking the end of the traversal
   */
  child_iterator children_end(loop_iterator &parent) const
    { child_iterator it(parent, child_iterator::END); return it; }
};

#endif /* _LOOP_NESTING_TREE_H */

