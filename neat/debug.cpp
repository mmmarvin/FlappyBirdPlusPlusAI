#include "debug.h"

#ifndef NDEBUG
#include <iostream>
#endif // NDEBUG

namespace NEAT
{
  std::ostream& neatDebugger()
  {
#ifndef NDEBUG
    return std::cout;
#else
    static nullstream instance;
    return instance;
#endif // NDEBUG
  }
}
