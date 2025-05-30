/*
 * Copyright (c) 2010 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jung Ho Ahn
 */

#ifndef __PTSCOMPONENT_H__
#define __PTSCOMPONENT_H__

#include "PTS.h"
#include <set>
#include <iostream>

using namespace std;

namespace PinPthread
{
  class Component;
  class GlobalEventQueue;

  enum component_type
  {
    ct_core,
    ct_lsu,
    ct_o3core,
    ct_o3core_t1,
    ct_o3core_t2,
    ct_cachel1d,
    ct_cachel1d_t1,
    ct_cachel1d_t2,
    ct_cachel1i,
    ct_cachel1i_t1,
    ct_cachel1i_t2,
    ct_cachel2,
    ct_cachel2_t1,
    ct_cachel2_t2,
    ct_cachel3,
    ct_cachel3_t1,
    ct_cachel3_t2,
    ct_directory,
    ct_crossbar,
    ct_rbol,
    ct_memory_controller,
    ct_tlbl1d,
    ct_tlbl1i,
    ct_tlbl2,
    ct_mesh,
    ct_ring,
  };

  enum event_type
  {
    et_read,
    et_write,
    et_write_nd,
    et_evict,
    et_evict_owned,
    et_evict_nd,
    et_dir_evict,
    et_e_to_m,
    et_e_to_i,
    et_e_to_s,
    et_e_to_s_nd,
    et_m_to_s,
    et_m_to_m,
    et_m_to_i,
    et_s_to_s,
    et_o_to_o,
    et_s_to_s_nd,
    et_o_to_o_nd,
    et_m_to_e,
    et_dir_rd,      // read request originated from a directory
    et_dir_rd_nd,
    et_e_rd,
    et_s_rd,
    et_nack,
    et_invalidate,  // originated from a directory
    et_invalidate_nd,
    et_s_rd_wr,     // when a directory state changes from modified to shared, data must be written
    et_rd_bypass,   // this read is older than what is in the cache
    et_tlb_rd,
    et_i_to_e,
    et_rd_dir_info_req,
    et_rd_dir_info_rep,
    et_nop,
  };

  struct LocalQueueElement
  {
    std::stack<Component *> from;  // where it is from
    event_type type;
    uint64_t   address;
    uint32_t   th_id;
    bool       dummy;
    uint64_t   prefetch; // prefetch ready time
    int32_t    rob_entry;

    LocalQueueElement() : from(), th_id(0), dummy(false), prefetch(0), rob_entry(-1) { }
    LocalQueueElement(Component * comp, event_type type_, uint64_t address_)
      : from(), type(type_), address(address_), th_id(0), dummy(false), prefetch(0), rob_entry(-1) { from.push(comp); }

    void display();
  };



  class Component  // meta-class
  {
    public:
      Component(component_type type_, uint32_t num_, McSim * mcsim_);
      virtual ~Component() { }

      uint32_t                 process_interval;
      component_type           type;
      uint32_t                 num;
      McSim                  * mcsim;
      GlobalEventQueue       * geq;  // global event queue

      virtual void add_req_event(uint64_t, LocalQueueElement *, Component * from) { ASSERTX(0); }
      virtual void add_rep_event(uint64_t, LocalQueueElement *, Component * from) { ASSERTX(0); }
      virtual void add_req_event(uint64_t a, LocalQueueElement * b) { add_req_event(a, b, NULL); }
      virtual void add_rep_event(uint64_t a, LocalQueueElement * b) { add_rep_event(a, b, NULL); }
      virtual uint32_t process_event(uint64_t curr_time) = 0;
      virtual void show_state(uint64_t address) { }
      virtual void display();

      std::multimap<uint64_t, LocalQueueElement *> req_event;
      std::multimap<uint64_t, LocalQueueElement *> rep_event;
      std::queue<LocalQueueElement *> req_q;
      std::queue<LocalQueueElement *> rep_q;

    protected:
      const char * prefix_str() const;
      uint32_t get_param_uint64(const string & param, uint32_t def = 0) const ;
      uint32_t get_param_uint64(const string & param, const string & prefix, uint32_t def = 0) const;
      string   get_param_str(const string & param) const;
      uint32_t log2(uint64_t num);
  };



  typedef std::map<uint64_t, std::set<Component *> > event_queue_t;

  class GlobalEventQueue
  {
    public:
    //private:
      event_queue_t event_queue;
      uint64_t curr_time;
      McSim * mcsim;

    public:
      GlobalEventQueue(McSim * mcsim_);
      ~GlobalEventQueue();
      void add_event(uint64_t event_time, Component *);
      uint32_t process_event();
      void display();


      uint32_t num_hthreads;
      uint32_t num_mcs;
      uint32_t interleave_base_bit;
      uint32_t interleave_xor_base_bit;
      uint32_t page_sz_base_bit;
      bool     is_asymmetric;
      uint32_t num_l3s;
      uint32_t set_lsb;
      uint32_t which_mc(uint64_t);  // which mc does an address belong to?

      uint32_t which_l3(uint64_t);
  };
}

#endif

