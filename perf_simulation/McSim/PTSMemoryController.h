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

#ifndef PTS_MEMORYCONTROLLER_H
#define PTS_MEMORYCONTROLLER_H

#include "McSim.h"
#include "PTSHash.h"
#include <list>
#include <stack>
#include <queue>
#include <vector>
#include <set>


// some of the acronyms used here
// ------------------------------
//  pd : powerdown
//  vmd : virtual memory device

using namespace std;

namespace PinPthread
{
  enum mc_bank_action
  {
    mc_bank_activate,
    mc_bank_read,
    mc_bank_write,
    mc_bank_precharge,
    mc_bank_idle,
    mc_bank_refresh,
  };

  enum mc_sched_policy
  {
    mc_sched_open,
    mc_sched_closed,
    mc_sched_l_pred,
    mc_sched_g_pred,
    mc_sched_tournament,
    mc_sched_m_open,  // minimalist open
    mc_sched_a_open,  // adaptive open
  };

  enum mc_pred_tournament_idx
  {
    mc_pred_open,
    mc_pred_closed,
    mc_pred_local,
    mc_pred_global,
    mc_pred_invalid,
  };

  enum rh_prevention_scheme
  {
    rh_none,        // Baseline (system w/o RowHammer mitigation)
                    // "RowArmor: Efficient and Comprehensive Protection Against DRAM Disturbance Attacks," ASPLOS, 2026
    rh_para,        // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
    rh_graphene,    // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
    rh_blockhammer, // "BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows," HPCA, 2021
    rh_hydra,       // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
    rh_srs,         // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
                    // "Randomized Row-Swap: Mitigating Row Hammer by Breaking Spatial Correlation between Aggressor and Victim Rows," ASPLOS, 2022
    rh_rampart,     // "RAMPART: RowHammer Mitigation and Repair for Sever Memory Systems," MEMSYS, 2023
    rh_abacus,      // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
    rh_prac,        // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
  };
  
  class Bliss {
    public:
      explicit Bliss(uint32_t threshold, uint32_t clearing_interval);
      void clear_blacklist();
      void update_blacklist(uint32_t th_id);
      bool is_blacklisted(uint32_t th_id);
      const uint32_t clearing_interval; // the blacklist information is cleared periodically
                                        // after every Clearing Interval (10000 cycles)
    private:
      uint32_t th_id;                   // the thread ID of the last scheduled request
      uint32_t num_req_served;          // the number of requests served from an application
      std::set<uint32_t> blacklist;     // <thread ID>
      const uint32_t threshold;         // blacklisting threshold
  };
  
  class RowHammerGen {
    public:
      RowHammerGen();
      explicit RowHammerGen(uint32_t, uint32_t, uint32_t);
      ~RowHammerGen() {}
      uint32_t attacker_thread_number;
      uint32_t num_attack_rows_per_thread;
      uint32_t attacker_max_req_count;  // limit
      uint32_t attacker_req_count;            // currently
      std::vector<uint64_t> address_list;
      uint32_t curr_idx;
      LocalQueueElement * make_event(Component * comp);
      uint64_t make_address(uint32_t rank, uint32_t bank, uint64_t page);
      uint32_t log2(uint32_t num);

      uint32_t num_ranks_per_mc;
      uint32_t num_banks_per_rank;
      uint32_t rank_interleave_base_bit;
      uint32_t bank_interleave_base_bit;
      uint32_t mc_interleave_base_bit;
      uint64_t page_sz_base_bit;
      uint32_t interleave_xor_base_bit;
      uint32_t num_mcs;
  };

  class MemoryController : public Component
  {
    public:
      class BankStatus
      {
        public:
          uint64_t action_time;
          uint64_t page_num;
          uint64_t th_id;                             // last accessed thread
          mc_bank_action action_type;
          mc_bank_action action_type_prev;
          uint64_t latest_activate_time;
          uint64_t latest_write_time;
          list< pair<uint64_t, bool> > cached_pages;  // page number, dirty
          vector<uint32_t> bimodal_entry;             // 0 -- strongly open
          uint32_t local_bimodal_entry;               // 0 -- strongly open
          bool     skip_pred;                         // do not update predictor states
          bool     rh_ref;                            // for rowhammer preventive refresh
          bool     rh_update;                         // for update
          bool     rh_swap;                           // for row-swap in RRS

          BankStatus(uint32_t num_entries): action_time(0),
              page_num(0),th_id(0),
              action_type(mc_bank_idle), action_type_prev(mc_bank_idle),
              latest_activate_time(0), latest_write_time(0), cached_pages(),
              bimodal_entry(num_entries, 0),
              local_bimodal_entry(0), skip_pred(false), rh_ref(false), rh_update(false), rh_swap(false)
          {
          }
      };

      // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
      class Graphene_entry {
       public:
        Graphene_entry();
        ~Graphene_entry();
        uint64_t get_address() { return address; }
        uint64_t get_count() { return count; }
        void increment_count() { count++; }
        void set_address(uint64_t address_) { address = address_; }
        void reset_address() { address = ((uint64_t)1) << 62; }
        void reset_count() { count = 0; }

       private:
        uint64_t address;
        uint64_t count;
      };

      class Graphene {
       public:
        Graphene(int size_, int threshold_) : size(size_), threshold(threshold_) { entries = new Graphene_entry[size]; }
        ~Graphene() {}
        Graphene_entry *entries;
        uint64_t get_address(int idx);
        uint64_t get_count(int idx);
        uint64_t activate(uint64_t addr);
        void debug();
        void flush();

       private:
        int threshold;
        int size; // the number of entries
      };
      vector<vector<Graphene>> graphene;
      vector<vector<uint64_t>> graphene_statistics;
      vector<uint64_t> graphene_flush_time;
      uint64_t graphene_flush_interval;
      uint64_t graphene_table_size;
      
      // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
      class Hydra {
       public:
        Hydra();
        Hydra(uint64_t hydra_gct_threshold, uint64_t hydra_gct_num_entries, uint64_t hydra_rcc_num_entries, 
            uint64_t hydra_rct_threshold, uint64_t num_max_rows, uint64_t page_sz_base_bit);
        ~Hydra();
        int activate(uint64_t curr_time, uint64_t address, uint32_t th_id);
        int update_rct(uint64_t address);
        void target_refresh_rct(uint64_t address);
        int query_gct(uint64_t address);
        int query_rcc(uint64_t address);
        void reset();
        uint64_t num_rct_access;
        uint64_t num_gct_access;
        uint64_t num_rcc_access;

       private:
        map<uint64_t, uint64_t> gct;
        deque<uint64_t> rcc;
        map<uint64_t, uint64_t> rct;
        uint64_t get_gct_idx(uint64_t address);
        uint64_t page_sz_base_bit;
        uint64_t group_base_bit;
        uint64_t hydra_gct_threshold, hydra_gct_num_entries;
        uint64_t hydra_rcc_num_entries;
        uint64_t hydra_rct_num_entries, hydra_rct_threshold;
        uint64_t num_max_rows;
      };
      uint64_t num_refreshes_for_hydra;
      uint64_t num_rct_updates;
      uint64_t hydra_ref;
      uint64_t hydra_update;
      Hydra *hydra;

      // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
      // "Randomized Row-Swap: Mitigating Row Hammer by Breaking Spatial Correlation between Aggressor and Victim Rows," ASPLOS, 2022
      class CAT {
        // we just use Graphene table
       public:
        CAT(uint64_t num_entry, uint32_t num_table);
        ~CAT();
        uint64_t insert(uint64_t addr);
        uint64_t get_value(uint64_t addr);
        void set_config_number(uint32_t mc_num, uint32_t rank_num, uint32_t bank_num);
        uint32_t get_table_idx(uint64_t addr);
        uint32_t get_set_idx(uint64_t addr, uint32_t tidx);
        void reset();

       private:
        vector<vector<vector<uint64_t>>> data;
        H3 *hash;
        uint64_t num_entries;
        uint32_t num_tables;
        uint32_t num_sets_per_table, num_ways_per_table;
        uint32_t num_demand_ways, num_extra_ways;
        uint32_t mc_num, rank_num, bank_num;
      };

      class RRS
      {
       public:
        RRS(uint64_t rrs_threshold, uint64_t rrs_hrt_threshold, uint32_t rit_num_entry, uint32_t hrt_num_entry, uint32_t rrs_num_tables);
        ~RRS();
        bool activate(uint64_t row_addr);
        bool update_hrt(uint64_t row_addr);
        void periodic_refresh();
        void debug();
        bool lazy_eviction(uint64_t time, uint64_t interval); // SRS implementation (HPCA, 2023)
        void set_lazy_eviction_time(uint64_t interval);

       private:
        Graphene_entry *entries;
        uint32_t rit_num_entry, hrt_num_entry;
        uint64_t rrs_threshold, rrs_hrt_threshold;
        uint32_t rrs_num_tables;
        uint32_t mc_num, rank_num, bank_num;
        uint64_t num_rows;
        uint64_t row_swap_counter;
        uint64_t spcnt;

       public:
        uint64_t lazy_eviction_count;
        uint64_t last_lazy_eviction_time;
      };
      vector<vector<RRS>> rrs;  // [rank][bank]
      uint64_t rrs_delay_rit_access, rrs_delay_row_swap;
      uint32_t rrs_num_tables;
      uint64_t rrs_hrt_num_entry, rrs_rit_num_entry;
      uint64_t rrs_threshold, rrs_hrt_threshold;
      uint64_t rrs_num_row_swap;
      uint64_t lazy_eviction_interval;
      uint64_t rrs_num_lazy_evictions;

      // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
      class ABACuS_entry {
        public:
         ABACuS_entry();
         ~ABACuS_entry();
         uint64_t get_row_id() const;
         uint64_t get_rac() const;
         uint64_t get_sav() const;
         void set_row_id(uint64_t row_id);
         void set_rac(uint64_t rac);
         void set_sav(uint64_t sav);
         void reset();
 
        private:
         uint64_t row_id_;
         uint64_t rac_;
         uint64_t sav_;
       };
 
       class ABACuS {
        public:
         ABACuS(uint64_t n, uint64_t p, uint64_t r);
         ~ABACuS();
         int activate(uint64_t bank_id, uint64_t row_id);
         void refresh_cycle();
         void periodic_refresh();
         void preventive_refresh();
         void debug(uint64_t rank);
 
        private:
         vector< ABACuS_entry > entries;
         uint64_t num_entries_;
         uint64_t spillover_counter_;
         uint64_t prt_;
         uint64_t rct_;
         uint64_t num_refresh_cycle_;
         uint64_t num_preventive_refreshes_;
         uint64_t num_periodic_refresh_;
       };
       vector< ABACuS* > abacus;
       uint64_t total_num_refresh_cycle = 0;
       uint64_t total_preventive_refersh = 0;

      // Refresh Management (RFM) -- RAA counter
      void update_RAA_counter(uint32_t rank_num, uint32_t bank_num);
      void show_RAA_counter();

      // "BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows," HPCA, 2021
      struct BlockHammerParameters {
        uint32_t num_ranks_per_mc;
        uint32_t num_banks_per_rank;
        // CBF (counting Bloom filters)
        uint32_t blistTH;
        uint32_t cntSize;
        uint32_t totNumHashes;
        // HB
        uint32_t historySize;
        float Delay;
        // hash function
        H3 * hash;
        //
        bool attackthrottler;
        float quota_base;
        uint64_t Nth;
      };

      class RowBlockerCBF {
        public:
          RowBlockerCBF(BlockHammerParameters &p);
          ~RowBlockerCBF();

          uint64_t update(uint64_t row_num); // returns -1 when no BL, an address when BL.
          uint64_t query(uint64_t row_num); // returns -1 when no BL, an address when BL.

          void reset(); // at every tREFW/2
          void validate(bool _valid);
          bool get_valid();

          void print_CBF(); // for debug
          uint64_t sum_CBF();

        private:
          std::vector<uint64_t> cntTable; // main table. i.e. Count-min Sketch table.
          bool valid;
          H3 * hash;

          uint32_t blistTH; // blacklist threshold
          uint32_t cntSize; // number of counter table entries
          uint32_t totNumHashes; // number of hash functions

          // perf counter
          uint64_t num_reset;

          /*
          uint64_t num_blacklisted_rows; // updated at every reset or destructor
          std::vector<uint64_t> blacklisted_rows; // <row_num> printed at destructor
          */
         
          uint32_t lookup_hash(uint64_t row_num, uint32_t hash_number); 
      };

      class RowBlockerHB {
        struct historyEntry {
          uint32_t bankNum;
          uint64_t pageNum;
        };

        public:
          RowBlockerHB(BlockHammerParameters &p);
          ~RowBlockerHB();

          // returns true if recently activated.
          bool update(uint64_t row_num,uint32_t bank_num,
              uint32_t rank_num,uint32_t num,uint64_t th_id,uint64_t curr_time); // rank_num, num are for debug
          bool query(uint64_t row_num, uint32_t bank_num, uint64_t th_id, uint64_t curr_time);
          void print_HB(); // for debug

        private:
          std::vector<std::multimap<uint64_t, historyEntry *>> historyBuffer; // [bank]<timestamp, entry>

          uint64_t historySize; // set to match (Delay / tRC)
          float Delay; // unit of tick
      };

      class AttackThrottler {
        public:
          AttackThrottler(BlockHammerParameters &p);
          ~AttackThrottler();

          void update_BL_ACTs(uint64_t th_id);
          void update_used_quota(uint64_t th_id, bool _increment); // ++ or --
          bool query(uint64_t th_id,uint64_t _rank_num,uint64_t _bank_num,uint64_t _num); // true if should be stopped. false if not.
          // _rank_num, _ban_num are for debug

          void reset();
          void validate(bool _valid);
          bool get_valid();

          void print_AttackThrottler();

        //private:
          bool valid;
          float quota_base;
          uint32_t blistTH; // TODO should change how the parameters are set
          uint64_t Nth;

          std::map<uint64_t,uint64_t> BL_ACTs; // <th_id, blacklisted row activation count>
          // quota = (1/RHLI)*(quota_base) - quota_base is empirical number
          std::map<uint64_t,uint64_t> used_quota; // <th_id, in-flight requests>
      };
     
      class BlockHammer {
        public:
          MemoryController * mc; // uplink
          bool attackthrottler; // true if on, false if turned-off

          BlockHammer(BlockHammerParameters &p);
          ~BlockHammer();

          // returns -1 when no BL, an address when BL. // update
          uint64_t ACT(uint64_t row_num,uint32_t rank_num,uint32_t bank_num,uint64_t _th_id,uint64_t curr_time);
          // returns -1 when no BL, an address when BL.
          uint64_t query_rowblocker(uint64_t row_num, uint32_t rank_num, uint32_t bank_num, uint64_t th_id, uint64_t curr_time);

          // 2 CBFs interleaved. [rank][bank]
          std::vector<std::vector<std::pair<RowBlockerCBF*,RowBlockerCBF*>>> rowblocker_CBFs;
          
          std::vector<RowBlockerHB *> rowblocker_HBs; // exists per rank. [rank]

          // 2 AttackThrottler's list interleaved.
          std::vector<std::vector<std::pair<AttackThrottler*,AttackThrottler*>>> attackthrottlers; // [rank][bank]

          // for debug
          void print_blockhammer();
          // for debug [rank][bank] activated row_num, number of activation
          std::vector< std::vector< std::map<uint64_t, uint64_t> > > tot_row_list;
          std::vector< std::vector<uint64_t> > tot_row_num; // [rank][bank] activated row num
          std::vector< std::vector<uint64_t> > tot_row_act; // [rank][bank] activation
          //
          uint64_t rowblocker_throttled;
      };

      // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
      // We calculate and set the para probability (p_para) considering the blast radius.
      // On each activation of PARA, we refreshes one selected row adjacent to the activated row.
      float p_para;

      // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
      vector<vector<vector<uint64_t>>> prac;
      
      // RowHammer configurations
      uint64_t num_rh;
      rh_prevention_scheme rh_mode;
      uint64_t th_RH;
      
      // "BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows," HPCA, 2021
      BlockHammer * my_bh; 

      // RowHammer attacks (for Denial of Service)
      bool use_attacker;
      uint32_t num_rowhammer_attackers;
      vector<RowHammerGen*> hammer_threads;

      MemoryController(component_type type_, uint32_t num_, McSim * mcsim_);
      ~MemoryController();

      void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      uint32_t process_event(uint64_t curr_time);

      Component * directory;  // uplink
      NoC * crossbar;
      vector<LocalQueueElement *> req_l;
      int32_t curr_batch_last;
      std::vector<uint64_t> act_from_a_th;
      std::vector<uint64_t> acc_from_a_th;
      std::vector<uint64_t> target_bank_act_from_a_th;

      Bliss* my_bliss;

      const uint32_t mc_to_dir_t;
      const uint32_t num_ranks_per_mc;
      const uint32_t num_banks_per_rank;

    private:
      // RowHammer configurations
      uint32_t blast_radius;
      // Refresh Management (RFM)
      uint32_t RAAIMT;
      uint64_t tRFM_t;
      vector<vector<bool>> todo_RFM_flag;      // [rank][bank]
      vector<vector<uint64_t>> RAA_counter;    // [rank][bank]
      vector<vector<uint64_t>> num_RFM;        // [rank][bank]
      vector<vector<uint64_t>> num_RFM_refresh; // [rank][bank]
      vector<vector<uint64_t>> unissued_RFM; // [rank][bank]
      vector<vector<uint64_t>> issued_RFM; // [rank][bank]

      // The unit of each t* value is "process_interval"
      // assume that RL = WL (in terms of DDRx)
      const uint32_t tRCD;
      const uint32_t tRR;
      uint32_t       tRP;
      uint32_t       tRTP;          // read to precharge
      uint32_t       tWTP;          // write to precharge
      const uint32_t tCL;           // CAS latency
      const uint32_t tBL;           // burst length
      const uint32_t tBBL;          // bank burst length
      const uint32_t tBBLW;         // tCCD for WR
      const uint32_t tRAS;          // activate to precharge
      // send multiple column level commands
      const uint32_t tWRBUB;        // WR->RD bubble between any ranks
      const uint32_t tRWBUB;        // RD->WR bubble between any ranks
      const uint32_t tRRBUB;        // RD->RD bubble between two different ranks
      const uint32_t tWTR;          // WR->RD time in the same rank
      const uint32_t tRTW;          // RD->WR time in the same rank
      const uint32_t tRTRS;         // rank-to-rank switching time
      const uint32_t tM_OPEN_TO;    // minimalist open time-out (unit: process_interval)
      // adaptive open policy related constraints
      const uint32_t tA_OPEN_TO_INIT; // adaptive open time-out initial value
      const uint32_t tA_OPEN_TO_DELTA;// adaptive open time-out changing step
      const uint32_t a_open_win_sz;   // # of reqs. that we track before making a decision to change time-out
      const uint32_t a_open_opc_th;   // overdue page close threshold
      const uint32_t a_open_ppc_th;   // premature page close threshold

      const uint32_t req_window_sz; // up to how many requests can be considered during scheduling
      uint32_t       interleave_xor_base_bit;

      // bank group (starting from DDR4) related constraints
      uint32_t num_bank_groups;     // bank group disabled if num_bank_groups = 1
      map<uint64_t, uint32_t> bank_group_status;  // < time, rank_id*num_bank_groups + bank_group_id >
      uint32_t tWRBUB_same_group;   // minimal distance between WRs to the same bank group
      uint32_t tRDBUB_same_group;   // minimal distance between RDs to the same bank group
      uint32_t max_BUB_distance;    // max(tWRBUB_same_group, tRDBUB_same_group)

      uint32_t mc_to_dir_t_ab;
      uint32_t tRCD_ab;
      uint32_t tRAS_ab;
      uint32_t tRP_ab;
      uint32_t tCL_ab;
      uint32_t tBL_ab;
      uint32_t num_banks_per_rank_ab;
      vector< vector<bool> > last_time_from_ab;
      uint32_t num_banks_with_agile_row;
      uint32_t reciprocal_of_agile_row_portion;
      uint32_t last_rank_num;              // previously access rank id
      uint32_t last_rank_acc_time;         // previous rank access time

    public:
      const uint32_t rank_interleave_base_bit;
      const uint32_t bank_interleave_base_bit;
      const uint64_t page_sz_base_bit;   // byte addressing
      const uint32_t mc_interleave_base_bit;
      const uint32_t num_mcs;
      bool           is_shared_llc;

    private:
      mc_sched_policy policy;
      bool           use_bank_group;
      bool           mini_rank;
      bool           par_bs;  // parallelism-aware batch-scheduling
      bool           bliss;   // thread-level scheduler
      uint64_t       refresh_interval;
      uint64_t       tRFC_t;         // tRFC in tick
      uint64_t       curr_refresh_page;
      uint64_t       curr_refresh_bank; // not used
      uint64_t       curr_refresh_rank;
      uint64_t       num_pages_per_bank;
      uint64_t       num_cached_pages_per_bank;
      bool           full_duplex;
      bool           is_fixed_latency;       // infinite BW
      bool           is_fixed_bw_n_latency;  // take care of BW as well
      bool           is_prediction_based;    // switch between closed and open using a prediction technique
      bool           display_page_acc_pattern;
      bool           not_sharing_banks;      // if yes, no threads share banks
      bool           cont_restore_after_activate;
      bool           cont_restore_after_write;
      bool           cont_precharge_after_activate;
      bool           cont_precharge_after_write;
      uint32_t bimodal_entry;               // global predictor : 0 -- strongly open
      uint32_t ** global_bimodal_entry;     // global predictor : 0 -- strongly open, [thread][history]
      uint64_t * pred_history;              // size : num_hthreads
      uint32_t addr_offset_lsb;
      uint32_t num_history_patterns;        // 2^(how many previous results to use for choosing history)
      uint32_t num_ecc_bursts;
      // adaptive open policy related parameters
      bool     a_open_fixed_to;       // use the fixed time-out when adaptive open
      uint32_t tA_OPEN_TO;            // adaptive open time-out
      uint32_t a_open_opc;            // overdue page close count 
      uint32_t a_open_ppc;            // premature page close count
      uint32_t a_open_win_cnt;        // # of reqs. in the window

      uint64_t num_read;
      uint64_t num_write;
      uint64_t num_activate;
      uint64_t num_restore;
      uint64_t num_precharge;
      uint64_t num_ab_read;
      uint64_t num_ab_write;
      uint64_t num_ab_activate;
      uint64_t num_write_to_read_switch;
      uint64_t num_refresh;  // a refresh command is applied to all VMD/BANK in a rank
      uint64_t num_pred_miss;
      uint64_t num_pred_hit;
      uint64_t num_global_pred_miss;
      uint64_t num_global_pred_hit;
      uint64_t accu_num_activated_bank;
      uint64_t last_call_process_event;

   public:
      uint64_t num_l_pred_miss_curr;
      uint64_t num_l_pred_hit_curr;
      uint64_t num_g_pred_miss_curr;
      uint64_t num_g_pred_hit_curr;
      uint64_t num_o_pred_miss_curr;
      uint64_t num_o_pred_hit_curr;
      uint64_t num_c_pred_miss_curr;
      uint64_t num_c_pred_hit_curr;
      uint64_t num_l_pred_miss;
      uint64_t num_l_pred_hit;
      uint64_t num_g_pred_miss;
      uint64_t num_g_pred_hit;
      uint64_t num_o_pred_miss;
      uint64_t num_o_pred_hit;
      uint64_t num_c_pred_miss;
      uint64_t num_c_pred_hit;
      uint64_t num_t_pred_miss;
      uint64_t num_t_pred_hit;

    private:
      mc_pred_tournament_idx curr_tournament_idx;  // 0 open, 1 closed, 2 local, 3 global
      uint64_t curr_best_predictor_idx;  // make the tournament selection into bimodal as well!!
      uint64_t tournament_interval;  // how many accesses will trigger tournament index reset? -- no prediction if this value is 0
      uint64_t num_acc_till_last_interval;
      uint32_t num_pred_entries;
      uint32_t base0,  base1,  base2;
      uint32_t width0, width1, width2;
      uint64_t last_process_time;
      uint64_t packet_time_in_mc_acc;
      vector< vector<BankStatus> > bank_status;         // [rank][bank]
      vector< uint64_t > last_activate_time;            // [rank]
      vector< uint64_t > last_write_time;               // [rank]
      pair< uint32_t, uint64_t > last_read_time;        // <rank, tick>
      vector< uint64_t > last_read_time_rank;           // [rank]
      vector< bool >     is_last_time_write;            // [rank]
      map<uint64_t, mc_bank_action> rd_dp_status;       // reuse (RD,WR,IDLE) BankStatus
      map<uint64_t, mc_bank_action> wr_dp_status;       // reuse (RD,WR,IDLE) BankStatus
      map<LocalQueueElement *, uint64_t> event_n_time;  // event pointer, arrival time

    public:
      map<uint64_t, uint64_t> os_page_acc_dist;       // os page access distribution
      map<uint64_t, uint64_t> os_page_acc_dist_curr;  // os page access distribution
      bool     display_os_page_usage_summary;
      bool     display_os_page_usage;
      uint64_t num_reqs;
      void     update_acc_dist();

    private:
      uint64_t get_page_num(uint64_t addr);
      uint64_t get_col_num(uint64_t addr);
      void show_state(uint64_t curr_time);

      bool     pre_processing(uint64_t curr_time);  // returns if the command was already sent or not.
      void     check_bank_status(LocalQueueElement * local_event);

      uint32_t num_hthreads;
      int32_t * num_req_from_a_th;

      void show_page_acc_pattern(uint64_t th_id, uint32_t rank, uint32_t bank,  uint64_t page, mc_bank_action type, uint64_t curr_time);
      inline uint32_t get_rank_num(uint64_t addr) { return ((addr >> rank_interleave_base_bit) ^ (addr >> interleave_xor_base_bit)) % num_ranks_per_mc; }
      inline uint32_t get_bank_num(uint64_t addr, uint64_t th_id) {
        uint32_t num_banks_per_rank_curr = (addr >> 63 != 0 && num_banks_per_rank_ab != 0) ?
                                           num_banks_per_rank_ab :
                                           (addr >> 63 != 0 && reciprocal_of_agile_row_portion == 0 ? num_banks_with_agile_row : num_banks_per_rank); 
        uint32_t bank_num = ((addr >> bank_interleave_base_bit) ^ (addr >> interleave_xor_base_bit)) % num_banks_per_rank_curr +
                            ((addr >> 63 != 0 && num_banks_per_rank_ab != 0) ? 
                             (addr >> 63 != 0 && reciprocal_of_agile_row_portion == 0 ? 0 : num_banks_per_rank) : 0);
        bank_num += (not_sharing_banks == true) ? num_banks_per_rank_curr*th_id : 0;
        return bank_num;
      }

    private:
      void current_print_parbs_batch();
  };

}

#endif

