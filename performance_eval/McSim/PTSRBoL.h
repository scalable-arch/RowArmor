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

#ifndef PTS_RBOL_H
#define PTS_RBOL_H

#include "McSim.h"
#include <list>
#include <stack>
#include <queue>
#include <set>

namespace PinPthread
{
  enum rbol_status_type
  {
    rst_valid,
    rst_dirty,
    rst_coming,  // from a memory controller
    rst_dirty_coming,  // similar to rst_coming but this will be rst_dirty
    rst_invalid,
  };


  class RBoL : public Component
  {
    public:
      uint32_t set_lsb;
      uint32_t rbol_set_lsb;
      uint32_t pred_set_lsb;
      uint32_t rbol_entry_lsb;
      uint32_t max_rbol_entries_to_prefetch;
      uint32_t num_rbol_sets;
      uint32_t num_rbol_ways;
      uint32_t num_pred_sets;
      uint32_t num_pred_ways;
      uint64_t to_dir_latest_time;
      uint64_t to_dir_hit_t;

      // stats
      uint64_t num_rd_pred_access;
      uint64_t num_wr_pred_access;
      uint64_t num_rbol_hit;
      uint64_t num_rbol_miss;
      uint64_t num_rbol_evict;

      vector< list< uint64_t > > pred_tags;
      vector< list< pair< uint64_t, rbol_status_type > > > rbol_tags;
      list<LocalQueueElement *> req_l;
      list<LocalQueueElement *> rep_l;

      RBoL(component_type type_, uint32_t num_, McSim * mcsim_);
      ~RBoL();

      MemoryController * mc;  // downlink
      Directory        * directory;         // uplink

      // assumption: rbol->mem and rbol->dir time is process_interval

      bool use_rbol;
      bool do_not_check_neighbors;
      bool adaptive_rbol_line;
      uint32_t max_prefetch_rbol_lines;
      bool * neighbor_existence;

      void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      uint32_t process_event(uint64_t curr_time);
      void show_state(uint64_t);
      void send_to_dir(uint64_t curr_time, LocalQueueElement *, bool hit = false);
  };
}

#endif

