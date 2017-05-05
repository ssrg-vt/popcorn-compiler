#include "llvm/Analysis/LoopNestingTree.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

LoopNestingTree::LoopNestingTree(const std::vector<BasicBlock *> &SCC,
                                 const LoopInfo &LI)
  : _size(1), _depth(1), _root(nullptr)
{
  unsigned depth = 1, nodeDepth;
  const Loop *loop = nullptr;
  Node *loopHeader = nullptr, *newHeader = nullptr;
  std::list<Node *> work;

  /* Bootstrap by grabbing the loop of the first basic block encountered. */
  loop = LI[SCC.front()];
  if(!loop) // Is the SCC actually a loop?
  {
    _root = new Node(SCC.front(), nullptr, true);
    return;
  }

  /* Get header of outermost loop, the tree's root. */
  while(loop->getLoopDepth() > 1)
    loop = loop->getParentLoop();
  _root = new Node(loop->getHeader(), nullptr, true);
  work.push_back(_root);

  /* Parse the loop-headers of the SCC into the tree. */
  while(work.size())
  {
    loopHeader = work.front();
    work.pop_front();
    loop = LI[loopHeader->bb];
    depth = LI.getLoopDepth(loopHeader->bb);
    _depth = (depth > _depth ? depth : _depth);

    /* Add children of the loop header. */
    for(auto bbi = loop->block_begin()++; bbi != loop->block_end(); bbi++)
    {
      nodeDepth = LI.getLoopDepth(*bbi);
      if(nodeDepth == depth) // Regular child node
      {
        loopHeader->addChild(new Node(*bbi, loopHeader, false));
        _size++;
      }
      else if(nodeDepth == (depth + 1) && // Header of nested loop
              LI.isLoopHeader(*bbi))
      {
        newHeader = new Node(*bbi, loopHeader, true);
        loopHeader->addChild(newHeader);
        work.push_back(newHeader);
        _size++;
      }
    }
  }
}

LoopNestingTree::loop_iterator LoopNestingTree::loop_iterator::operator++()
{
  loop_iterator me = *this;
  if(remaining.size())
  {
    cur = remaining.front();
    remaining.pop();
    addLoopHeaders();
  }
  else cur = nullptr;
  return me;
}

LoopNestingTree::loop_iterator
LoopNestingTree::loop_iterator::operator++(int junk)
{
  if(remaining.size())
  {
    cur = remaining.front();
    remaining.pop();
    addLoopHeaders();
  }
  else cur = nullptr;
  return *this;
}

LoopNestingTree::child_iterator::child_iterator(loop_iterator &parent,
                                                enum location loc)
{
  if(loc == BEGIN) it = parent.cur->children.begin();
  else it = parent.cur->children.end();
}

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

void LoopNestingTree::print(raw_ostream &O, Node *node, unsigned depth) const
{
  for(unsigned i = 0; i < depth; i++) O << " ";
  node->bb->printAsOperand(O, false);
  O << "\n";
  if(node->children.size())
  {
    for(unsigned i = 0; i < depth; i++) O << " ";
    O << "\\\n";

    for(std::list<Node *>::const_iterator it = node->children.begin();
        it != node->children.end();
        it++)
    {
      if((*it)->isLoopHeader) print(O, (*it), depth + 1);
      else
      {
        for(unsigned i = 0; i < depth + 1; i++) O << " ";
        (*it)->bb->printAsOperand(O, false);
        O << "\n";
      }
    }
  }
}

void LoopNestingTree::deleteRecursive(Node *node)
{
  for(std::list<Node *>::iterator it = node->children.begin();
      it != node->children.end();
      it++)
  {
    if((*it)->isLoopHeader) deleteRecursive(*it);
    else delete *it;
  }
  delete node;
}

void LoopNestingTree::loop_iterator::addLoopHeaders()
{
  if(cur != nullptr)
  {
    for(std::list<Node *>::const_iterator it = cur->children.begin();
        it != cur->children.end();
        it++)
      if((*it)->isLoopHeader)
        remaining.push(*it);
  }
}

