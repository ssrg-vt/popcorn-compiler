#include <fstream>
#include <iostream>
#include <mutex>

#include <cstdlib>
#include <execinfo.h>

#include "stack_depth.h"

#define STACK_DATA_FN_ENV "STACK_DATA_FILENAME"
#define STACK_DATA_MAX_DEPTH 512

using namespace std;

/* Call data */
static unordered_map<void*, FuncInfo>* funcCalls;
static mutex funcCallsLock;
static thread_local size_t stackDepth = 0;

void __attribute__((constructor))
__stack_depth_ctor(void)
{
  funcCalls = new unordered_map<void*, FuncInfo>();
}

void __attribute__((destructor))
__stack_depth_dtor(void)
{
  string fileName("stack_data.dat");
  ofstream stackData;
  unordered_map<void*, FuncInfo>::const_iterator it;

  if(getenv(STACK_DATA_FN_ENV)) fileName = getenv(STACK_DATA_FN_ENV);
  stackData.open(fileName.c_str());
  if(stackData.is_open()) {
    for(it = funcCalls->begin(); it != funcCalls->end(); it++)
      stackData << "(" << it->first << ", " << it->second.toStr() << ")" << endl;
    stackData.close();
  }
  else cerr << "[Stack-Depth] ERROR: could not open file " << fileName << endl;

  delete funcCalls;
}

extern "C" {

void __cyg_profile_func_enter(void* func, void* caller)
{
  unordered_map<void*, FuncInfo>::iterator ci;
  unordered_map<void*, uint64_t>::iterator ri;
  FuncInfo funcInfo;

  stackDepth++;
  funcCallsLock.lock();
  ci = funcCalls->find(func);
  if(ci != funcCalls->end())
  {
    ci->second.numCalls++;
    ci->second.avgStackDepth += stackDepth;
    if(stackDepth > ci->second.maxDepth.second)
    {
      ci->second.maxDepth.first = caller;
      ci->second.maxDepth.second = stackDepth;
    }

    ri = ci->second.caller.find(caller);
    if(ri != ci->second.caller.end()) ri->second++;
    else ci->second.caller.insert(unordered_map<void*, uint64_t>::value_type(caller, 1));
  }
  else
  {
    funcInfo.numCalls = 1;
    funcInfo.avgStackDepth = stackDepth;
    funcInfo.maxDepth.first = caller;
    funcInfo.maxDepth.second = stackDepth;
    funcInfo.caller.insert(unordered_map<void*, uint64_t>::value_type(caller, 1));
    funcCalls->insert(unordered_map<void*, FuncInfo>::value_type(func, funcInfo));
  }
  funcCallsLock.unlock();
}

void __cyg_profile_func_exit(void* dest, void* caller)
{
  stackDepth--;
}

}

