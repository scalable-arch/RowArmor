#include "PthreadMalloc.h"

using namespace PinPthread;

/* --------------------------------------------------------------------------- */
/* PthreadMalloc Constructor and Destructor:                                   */
/* --------------------------------------------------------------------------- */

PthreadMalloc::PthreadMalloc()
  :enablecontextswitch(true), depth(0)
{
}

PthreadMalloc::~PthreadMalloc()
{
}

/* --------------------------------------------------------------------------- */
/* Analyze:                                                                    */
/* analyze the calls and returns to determine whether we are thread-safe       */
/* upon entering an unsafe routine, disable context switching                  */
/* upon leaving an unsafe routine, disable context switching                   */
/* --------------------------------------------------------------------------- */

void PthreadMalloc::Analyze(pthread_t _current, bool iscall, bool istailcall,
    const string * rtn_name) 
{
  if (enablecontextswitch) 
  {
    ASSERTX(depth == 0);
    if (iscall && IsMalloc(rtn_name)) 
    {
#if VERYVERYVERBOSE
      std::cout << "Disable context switch upon rtn " << *rtn_name << "\n" << flush;
      std::cout << "Thread " << dec << _current << "\n" << flush;
#endif
      enablecontextswitch = false;
      depth++;
      current = _current;
    }
  }
  else if (rtn_name->find("pthread") == string::npos || rtn_name->find("_pthread") != string::npos)
  {
    ASSERTX(depth > 0);
    ASSERT(current == _current, "current = " + decstr((unsigned int)current) + ", _current = " + decstr((unsigned int)_current) + "\n");
    if (iscall && !istailcall) 
    {
      depth++;
    }
    else if (((istailcall) && (depth == 1) && !IsMalloc(rtn_name)) ||
        !iscall)
    {
      depth--;
      if (depth == 0) 
      {
#if VERYVERYVERBOSE
        std::cout << "Enable context switch upon rtn " << *rtn_name << "\n" << flush;
        std::cout << "Thread " << dec << _current << "\n" << flush;
        if (istailcall) 
        {
          std::cout << "(tailcall)\n" << flush;
        }
#endif
        enablecontextswitch = true;
      }
    }
  }
#if VERYVERBOSE
  if (iscall) 
  {
    std::cout << "Thread " << _current << " Calls RTN\t" << *rtn_name << " " << depth << "\n" << flush;
    if (false)
    {
      std::cout << "HOOK!\n" << flush;
    }
  }
  else
  {
    std::cout << "Thread " << _current << " Returns in RTN\t" << *rtn_name << " " << depth << "\n" << flush;
    if (false)
    {
      std::cout << "HOOK!\n" << flush;
    }
  }
#endif
}

/* --------------------------------------------------------------------------- */
/* CanSwitchThreads:                                                           */
/* determine whether it is safe to switch threads                              */
/* --------------------------------------------------------------------------- */

bool PthreadMalloc::CanSwitchThreads() 
{
  return enablecontextswitch;
}

/* --------------------------------------------------------------------------- */
/* IsMalloc:                                                                   */
/* determine whether the routine does unsafe memory allocation/deallocation    */
/* --------------------------------------------------------------------------- */

bool PthreadMalloc::IsMalloc(const string * rtn_name) 
{
  if (((rtn_name->find("__libc_malloc") != string::npos) ||
        (rtn_name->find("_int_malloc") != string::npos) ||
        (rtn_name->find("calloc") != string::npos) ||
        (rtn_name->find("realloc") != string::npos)|| 
        (rtn_name->find("__cfree") != string::npos) ||
        (rtn_name->find("_int_free") != string::npos) ||
        (rtn_name->find("istream") != string::npos) ||
        (rtn_name->find("ostream") != string::npos) ||
        (rtn_name->find("fstream") != string::npos) ||
        //(rtn_name->find("exit") != string::npos) ||
        (rtn_name->find("printf") != string::npos) ||
        (rtn_name->find("malloc") != string::npos) ||
        (rtn_name->find("mmap") != string::npos)/* ||
                                                   (rtn_name->find(".plt") != string::npos)*/) &&
      rtn_name->find("g_") == string::npos &&
      rtn_name->find("iostream") == string::npos)  // don't simulate libc functions  
  {
    return true;
  }
  return false;
}
