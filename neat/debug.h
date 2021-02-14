#ifndef DEBUG_H
#define DEBUG_H

#include <ostream>

namespace NEAT
{
#ifdef NDEBUG
  class nullstream : public std::ostream
  {
  public:
    nullstream() {}
  };
#endif // NDEBUG

  std::ostream& neatDebugger();
}

#endif // DEBUG_H
