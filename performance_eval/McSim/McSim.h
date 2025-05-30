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

#ifndef __MCSIM_H__
#define __MCSIM_H__

#include "PTS.h"
#include "PTSComponent.h"
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <vector>
#include <stdlib.h>


using namespace std;

const static ADDRINT search_addr   = 0x73db40;//0x800e7fffde77a040;
const static uint32_t max_hthreads = 1024;

namespace PinPthread 
{
  class Core;
  class O3Core;
  class Hthread;
  class CacheL1;
  class CacheL2;
  class CacheL3;
  class Directory;
  class RBoL;
  class Crossbar;
  class MemoryController;
  class BranchPredictor;
  class TLBL1;
  class TLBL2;
  class NoC;
  class McSim;


  enum ins_type
  {
    mem_rd,
    mem_2nd_rd,
    mem_wr,
    no_mem,
    ins_branch_taken,
    ins_branch_not_taken,
    ins_lock,
    ins_unlock,
    ins_barrier,
    ins_x87,
    ins_notify,   // for thread migration
    ins_waitfor,  // for thread migration
    ins_prefetch, // prefetch
    ins_invalid
  };

  enum coherence_state_type
  {
    cs_invalid,
    cs_shared,
    cs_exclusive,
    cs_modified,
    cs_owned,
    cs_tr_to_i,  // will be the invalid state
    cs_tr_to_s,  // will be the shared state
    cs_tr_to_m,  // will be the modified state
    cs_tr_to_e,
    cs_tr_to_o,
    cs_m_to_o,   // gajh: not sure if this will be used
    cs_m_to_s    // modified -> shared
  };



  class McSim
  {
    public:
      McSim(PthreadTimingSimulator * pts_);
      ~McSim();

      pair<uint32_t, uint64_t> resume_simulation(bool must_switch);
      uint32_t add_instruction(
          uint32_t hthreadid_,
          uint64_t curr_time_,
          uint64_t waddr,
          UINT32   wlen,
          uint64_t raddr,
          uint64_t raddr2,
          UINT32   rlen,
          uint64_t ip,
          uint32_t category,
          bool     isbranch,
          bool     isbranchtaken,
          bool     islock,
          bool     isunlock,
          bool     isbarrier,
          uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
          uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3
          );  // return value -- whether we have to resume simulation
      void link_thread(int32_t pth_id, bool * active_, int32_t * spinning_, ADDRINT * stack_, ADDRINT * stacksize_);
      void set_stack_n_size(int32_t pth_id, ADDRINT stack, ADDRINT stacksize);
      void set_active(int32_t pth_id, bool is_active);

      PthreadTimingSimulator   * pts;
      bool     skip_all_instrs;
      bool     simulate_only_data_caches;
      bool     show_l2_stat_per_interval;
      bool     is_race_free_application;
      uint32_t max_acc_queue_size;
      uint32_t num_hthreads;
      uint64_t print_interval;
      bool     use_o3core;
      bool     use_rbol;
      bool     is_asymmetric;
      bool     is_shared_llc;

      vector<Core *>             cores;
      vector<Hthread *>          hthreads;
      vector<O3Core *>           o3cores;
      vector<bool>               is_migrate_ready;
      vector<CacheL1 *>          l1ds;
      vector<CacheL1 *>          l1is;
      vector<CacheL2 *>          l2s;
      vector<CacheL3 *>          l3s;
      NoC *                      noc;
      vector<Directory *>        dirs;
      vector<RBoL *>             rbols;
      vector<MemoryController *> mcs;
      GlobalEventQueue *         global_q;
      vector<TLBL1 *>            tlbl1ds;
      vector<TLBL1 *>            tlbl1is;
      list<Component *>          comps;
      map<uint64_t, uint32_t>    wait_all;    // for barriers
      map<uint32_t, uint64_t>    notify_all;  // for barriers

      uint32_t get_num_hthreads() const { return num_hthreads; }
      uint64_t get_curr_time() const    { return global_q->curr_time; }
      void     show_state(uint64_t);
      void     show_l2_cache_summary();

      map<uint64_t, uint64_t>   os_page_req_dist; 
      void update_os_page_req_dist(uint64_t addr);
      uint64_t num_fetched_instrs;

      // some stat info
    private:
      uint64_t num_instrs_printed_last_time;

      uint64_t num_destroyed_cache_lines_last_time;
      uint64_t cache_line_life_time_last_time;
      uint64_t time_between_last_access_and_cache_destroy_last_time;

      uint64_t lsu_process_interval;
      uint64_t curr_time_last;
      uint64_t num_fetched_instrs_last;
      uint64_t num_mem_acc_last;
      uint64_t num_used_pages_last;
      uint64_t num_l1_acc_last;
      uint64_t num_l1_miss_last;
      uint64_t num_l2_acc_last;
      uint64_t num_l2_miss_last;
      uint64_t num_dependency_distance_last;
      uint64_t num_l3_acc_last;
      uint64_t num_l3_miss_last;
  };

}

#endif

