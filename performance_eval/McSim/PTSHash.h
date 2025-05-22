

#ifndef PTS_HASH_H
#define PTS_HASH_H

#include "McSim.h"
//#include <cstdint>
#include <limits>


namespace PinPthread {

  class H3 {
    public:
      H3();
      ~H3();

      uint32_t lookup_hash(uint64_t address, uint32_t hash_number);

    private:
      inline uint64_t bits(uint64_t addr, uint64_t first, uint64_t last) {
        assert(first >= last);
        uint32_t nbits = first - last + 1; 
        return (addr >> last) & mask(nbits);
      };

      inline uint64_t mask(uint64_t nbits) {
        return (nbits >= 64) ? (uint64_t)-1LL : (1ULL << nbits) - 1;
      };
  };

}

#endif
