/*
 * Definitions and structures to hold call information.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/10/2016
 */

#include <unordered_map>
#include <sstream>
#include <cstdint>

/*
 * Function call information.
 */
class FuncInfo
{
public:
  uint64_t numCalls;
  uint64_t avgStackDepth;
  std::pair<void*, uint64_t> maxDepth;
  std::unordered_map<void*, uint64_t> caller;

  FuncInfo() : numCalls(0), avgStackDepth(0), maxDepth(0, 0) {};

  std::string toStr(void) const
  {
    double avgDepth;
    std::stringstream ss;
    std::unordered_map<void*, uint64_t>::const_iterator it;

    avgDepth = (double)avgStackDepth / (double)numCalls;
    ss << numCalls << ", "
       << avgDepth << ", "
       << "(" << maxDepth.first << ", " << maxDepth.second << "), [";
    it = caller.begin();
    ss << "(" << it->first << ", " << it->second << ")";
    for(it++; it != caller.end(); it++)
      ss << ", (" << it->first << ", " << it->second << ")";
    ss << "]";

    return ss.str();
  }
};

