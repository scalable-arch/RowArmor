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

#include "PTSMemoryController.h"
#include "PTSXbar.h"
#include "PTSDirectory.h"
#include "PTSHash.h"

#include <iomanip>
#include <cmath>
#include <algorithm>

using namespace PinPthread;

ostream & operator << (ostream & output, mc_bank_action action)
{
  switch (action)
  {
    case mc_bank_activate:  output << "ACT"; break;
    case mc_bank_read:      output << "RD";  break;
    case mc_bank_write:     output << "WR";  break;
    case mc_bank_precharge: output << "PRE"; break;
    case mc_bank_idle:      output << "IDL"; break;
    case mc_bank_refresh:   output << "REF"; break;
    default: break;
  }
  return output;
}

ostream & operator << (ostream & output, mc_sched_policy policy)
{
  switch (policy)
  {
    case mc_sched_open:   output << "open"; break;
    case mc_sched_closed: output << "closed"; break;
    case mc_sched_l_pred: output << "l_pred"; break;
    case mc_sched_g_pred: output << "g_pred"; break;
    case mc_sched_tournament: output << "tour"; break;
    case mc_sched_m_open: output << "m_open"; break;
    case mc_sched_a_open: output << "a_open"; break;
    default: break;
  }
  return output;
}

ostream & operator << (ostream & output, mc_pred_tournament_idx idx)
{
  switch (idx)
  {
    case mc_pred_open:   output << "open"; break;
    case mc_pred_closed: output << "closed"; break;
    case mc_pred_local:  output << "local"; break;
    case mc_pred_global: output << "global"; break;
    default: break;
  }
  return output;
}

MemoryController::MemoryController(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
:Component(type_, num_, mcsim_),
  req_l(),
  mc_to_dir_t  (get_param_uint64("to_dir_t", 1000)),
  num_ranks_per_mc (get_param_uint64("num_ranks_per_mc", 1)),
  num_banks_per_rank(get_param_uint64("num_banks_per_rank", 8)),
  tRCD         (get_param_uint64("tRCD", 10)),
  tRR          (get_param_uint64("tRR",  5)),
  tRP          (get_param_uint64("tRP",  10)),
  tRTP         (get_param_uint64("tRTP", 10)),
  tWTP         (get_param_uint64("tWTP", 10)),
  tCL          (get_param_uint64("tCL",  10)),
  tBL          (get_param_uint64("tBL",  10)),
  tBBL         (get_param_uint64("tBBL", tBL)),
  tBBLW        (get_param_uint64("tBBLW", tBL)),
  tRAS         (get_param_uint64("tRAS", 15)),
  tWRBUB       (get_param_uint64("tWRBUB", 0)),
  tRWBUB       (get_param_uint64("tRWBUB", 0)),
  tRRBUB       (get_param_uint64("tRRBUB", 0)),
  tWTR         (get_param_uint64("tWTR", 8)),
  tRTW         (get_param_uint64("tRTW", 2)),
  tRTRS        (get_param_uint64("tRTRS", 4)),
  tM_OPEN_TO   (get_param_uint64("M_OPEN_TO", tRAS+tRP)),
  tA_OPEN_TO_INIT   (get_param_uint64("tA_OPEN_TO_INIT", 500)),
  tA_OPEN_TO_DELTA  (get_param_uint64("tA_OPEN_TO_DELTA", 50)),
  a_open_win_sz(get_param_uint64("a_open_win_sz", 64)),
  a_open_opc_th(get_param_uint64("a_open_opc_th", 6)),
  a_open_ppc_th(get_param_uint64("a_open_ppc_th", 6)),
  req_window_sz(get_param_uint64("req_window_sz", 16)),
  num_bank_groups(get_param_uint64("num_bank_groups", 1)),
  bank_group_status(),
  tWRBUB_same_group(get_param_uint64("tWRBUB_same_group", tBBL)),
  tRDBUB_same_group(get_param_uint64("tRDBUB_same_group", tBBL)),
  mc_to_dir_t_ab(get_param_uint64("mc_to_dir_t_ab", mc_to_dir_t)),
  tRCD_ab      (get_param_uint64("tRCD_ab", tRCD)),
  tRAS_ab      (get_param_uint64("tRAS_ab", tRAS)),
  tRP_ab       (get_param_uint64("tRP_ab",  tRP)),
  tCL_ab       (get_param_uint64("tCL_ab",  tCL)),
  tBL_ab       (get_param_uint64("tBL_ab",  tBL)),
  last_rank_num(-1), last_rank_acc_time(0),
  num_banks_per_rank_ab(get_param_uint64("num_banks_per_rank_ab", 0)),
  last_time_from_ab(num_ranks_per_mc, vector<bool>(num_banks_per_rank, false)),
  rank_interleave_base_bit(get_param_uint64("rank_interleave_base_bit", 14)),
  bank_interleave_base_bit(get_param_uint64("bank_interleave_base_bit", 14)),
  page_sz_base_bit        (get_param_uint64("page_sz_base_bit", 12)),
  mc_interleave_base_bit(get_param_uint64("interleave_base_bit", 12)),
  num_mcs(get_param_uint64("num_mcs", "pts.", 2)),
  num_read(0), num_write(0), num_activate(0), num_restore(0), num_precharge(0),
  num_ab_read(0), num_ab_write(0), num_ab_activate(0),
  num_write_to_read_switch(0), num_refresh(0), 
  num_pred_miss(0), num_pred_hit(0), num_global_pred_miss(0), num_global_pred_hit(0),
  accu_num_activated_bank(0),
  last_call_process_event(0),
  num_l_pred_miss(0), num_l_pred_hit(0), num_l_pred_miss_curr(0), num_l_pred_hit_curr(0),
  num_g_pred_miss(0), num_g_pred_hit(0), num_g_pred_miss_curr(0), num_g_pred_hit_curr(0),
  num_o_pred_miss(0), num_o_pred_hit(0), num_o_pred_miss_curr(0), num_o_pred_hit_curr(0),
  num_c_pred_miss(0), num_c_pred_hit(0), num_c_pred_miss_curr(0), num_c_pred_hit_curr(0),
  num_t_pred_miss(0), num_t_pred_hit(0),
  curr_best_predictor_idx(0),
  tournament_interval(get_param_uint64("tournament_interval", 0)),
  num_acc_till_last_interval(0),
  num_pred_entries(get_param_uint64("num_pred_entries", 1)),
  last_activate_time(num_ranks_per_mc, 0),
  last_write_time(num_ranks_per_mc, 0),
  last_read_time(pair<uint32_t, uint64_t>(0, 0)),
  last_read_time_rank(num_ranks_per_mc, 0),
  is_last_time_write(num_ranks_per_mc, false),
  rd_dp_status(),
  wr_dp_status(),
  RAA_counter(num_ranks_per_mc, vector<uint64_t>(num_banks_per_rank,0)),
  todo_RFM_flag(num_ranks_per_mc, vector<bool>(num_banks_per_rank)),
  num_RFM(num_ranks_per_mc, vector<uint64_t>(num_banks_per_rank,0)),
  num_RFM_refresh(num_ranks_per_mc, vector<uint64_t>(num_banks_per_rank,0)),
  unissued_RFM(num_ranks_per_mc, vector<uint64_t>(num_banks_per_rank,0)),
  issued_RFM(num_ranks_per_mc, vector<uint64_t>(num_banks_per_rank,0)),
  RAAIMT  (get_param_uint64("RAAIMT", 0)),
  tRFM_t  (get_param_uint64("tRFM_t", 0))
{
  /* Blast Radius implementation */
  blast_radius = get_param_uint64("blast_radius", 3);
  /* end of Blast Radius implenetation */
  cout << "RAAIMT: " << RAAIMT << " tRFM_t: " << tRFM_t << endl;
  process_interval = get_param_uint64("process_interval", 10);
  refresh_interval = get_param_uint64("refresh_interval",  0);
  tRFC_t = get_param_uint64("tRFC_t", 0);
  if (refresh_interval%(num_ranks_per_mc*process_interval) != 0) 
  {
    cout << "refresh_interval\%(num_ranks_per_mc*process_interval) != 0" << endl;
    exit(1);
  }
  if (refresh_interval != 0)
  {
    geq->add_event(refresh_interval/num_ranks_per_mc, this);
  }

  curr_refresh_page = 0;
  //curr_refresh_bank = 0;  // not used
  curr_refresh_rank = 0;
  num_pages_per_bank = get_param_uint64("num_pages_per_bank", 8192);
  num_cached_pages_per_bank = get_param_uint64("num_cached_pages_per_bank", 4);
  interleave_xor_base_bit = get_param_uint64("interleave_xor_base_bit", 20);
  addr_offset_lsb = get_param_uint64("addr_offset_lsb", "", 48);

  if (get_param_str("scheduling_policy") == "open")
  {
    policy = mc_sched_open;
  }
  else if (get_param_str("scheduling_policy") == "l_pred")
  {
    policy = mc_sched_l_pred;
  }
  else if (get_param_str("scheduling_policy") == "g_pred")
  {
    policy = mc_sched_g_pred;
  }
  else if (get_param_str("scheduling_policy") == "tournament")
  {
    policy = mc_sched_tournament;
  }
  else if (get_param_str("scheduling_policy") == "m_open")
  {
    policy = mc_sched_m_open;
  }
  else if (get_param_str("scheduling_policy") == "a_open")
  {
    policy = mc_sched_a_open;
  }
  else
  {
    policy = mc_sched_closed;
  }

  // variables below are used to find page_num quickly
  multimap<uint32_t, uint32_t> interleavers;
  interleavers.insert(pair<uint32_t, uint32_t>(rank_interleave_base_bit, num_ranks_per_mc));
  interleavers.insert(pair<uint32_t, uint32_t>(bank_interleave_base_bit, num_banks_per_rank));
  interleavers.insert(pair<uint32_t, uint32_t>(mc_interleave_base_bit,   num_mcs));

  multimap<uint32_t, uint32_t>::iterator iter = interleavers.begin();
  base2 = iter->first; width2 = iter->second; ++iter;
  base1 = iter->first; width1 = iter->second; ++iter;
  base0 = iter->first; width0 = iter->second; ++iter;

  par_bs      = get_param_str("par_bs")      == "true" ? true : false;
  bliss       = get_param_str("bliss")       == "true" ? true : false;
  full_duplex = get_param_str("full_duplex") == "true" ? true : false;
  use_bank_group        = get_param_str("use_bank_group") == "true" ? true : false;
  is_fixed_latency      = get_param_str("is_fixed_latency") == "true" ? true : false;
  is_fixed_bw_n_latency = get_param_str("is_fixed_bw_n_latency") == "true" ? true : false;
  is_prediction_based   = get_param_str("is_prediction_based") == "true" ? true : false;
  not_sharing_banks     = get_param_str("not_sharing_banks") == "true" ? true : false;
  is_shared_llc = mcsim->pts->get_param_str("is_shared_llc") == "true" ? true : false;
//  is_shared_llc         = get_param_str("is_shared_llc") == "true" ? true : false;
  cont_restore_after_activate = get_param_str("cont_restore_after_activate") == "false" ? false : true;  // true by default
  cont_restore_after_write    = get_param_str("cont_restore_after_write")    == "false" ? false : true;  // true by default
  cont_precharge_after_activate = get_param_str("cont_precharge_after_activate") == "true" ? true : false;
  cont_precharge_after_write    = get_param_str("cont_precharge_after_write")    == "true" ? true : false;
  display_page_acc_pattern = get_param_str("display_page_acc_pattern") == "true" ? true : false;
  bimodal_entry         = 0;

  if (cont_precharge_after_activate == true && cont_restore_after_activate == false) { ASSERTX(0); }
  if (cont_precharge_after_write    == true && cont_restore_after_write    == false) { ASSERTX(0); }

  if (not_sharing_banks == false)
  { 
    bank_status.insert(bank_status.begin(), num_ranks_per_mc, vector<BankStatus>(num_banks_per_rank + num_banks_per_rank_ab, BankStatus(num_pred_entries)));
  }
  else
  {
    bank_status.insert(bank_status.begin(), num_ranks_per_mc, vector<BankStatus>((num_banks_per_rank + num_banks_per_rank_ab)*mcsim->get_num_hthreads(), BankStatus(num_pred_entries)));
  }

  display_os_page_usage = get_param_str("display_os_page_usage") == "true" ? true : false;
  num_reqs              = 0;
  num_banks_with_agile_row = get_param_uint64("num_banks_with_agile_row", 0);
  reciprocal_of_agile_row_portion = get_param_uint64("reciprocal_of_agile_row_portion", 1);
  num_history_patterns = get_param_uint64("num_history_patterns", 1);

  curr_batch_last       = -1;
  num_hthreads          = mcsim_->get_num_hthreads();
  global_bimodal_entry  = new uint32_t *[num_hthreads];
  pred_history          = new uint64_t[num_hthreads];
  num_req_from_a_th     = new int32_t[num_hthreads];
  for (uint32_t i = 0; i < num_hthreads; i++)
  {
    acc_from_a_th.push_back(0);
    act_from_a_th.push_back(0);
    target_bank_act_from_a_th.push_back(0);
    num_req_from_a_th[i] = 0;
    pred_history[i]      = 0;
    global_bimodal_entry[i] = new uint32_t[num_history_patterns];
    for (uint32_t j = 0; j < num_history_patterns; j++)
    {
      global_bimodal_entry[i][j] = 0;
    }
  }

  last_process_time     = 0;
  packet_time_in_mc_acc = 0;

  max_BUB_distance = (tWRBUB_same_group > tRDBUB_same_group) ? tWRBUB_same_group : tRDBUB_same_group;
  num_ecc_bursts = get_param_uint64("num_ecc_bursts",0)*tBL*2;
  a_open_fixed_to = get_param_str("a_open_fixed_to") == "false" ? false : true;  // true by default
  tA_OPEN_TO = tA_OPEN_TO_INIT;
  a_open_opc = 0;
  a_open_ppc = 0;
  a_open_win_cnt = a_open_win_sz;

  // RowHammer related parameters
  num_rh = 0;
  th_RH = get_param_uint64("th_RH", 32768);

  // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
  if (get_param_str("rh_prevention_scheme") == "para") {
    rh_mode = rh_para;
    p_para = float(get_param_uint64("p_para", 100)) / 10000;
    srand((unsigned int)time(NULL));
    cout << "[PARA] Set p_para to " << p_para << "\n";
  }
  // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
  if (get_param_str("rh_prevention_scheme") == "graphene") {
    rh_mode = rh_graphene;
    graphene_flush_interval = get_param_uint64("graphene_flush_interval", 1);
    graphene_table_size = get_param_uint64("graphene_table_size", 0);
    graphene_flush_time.push_back(0);
    graphene_flush_time.push_back(4096);
    graphene.insert(graphene.begin(), num_ranks_per_mc, vector<Graphene>(num_banks_per_rank, Graphene(graphene_table_size, RAAIMT)));
    graphene_statistics.insert(graphene_statistics.begin(), num_ranks_per_mc, vector<uint64_t>(num_banks_per_rank, 0));
    for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        Graphene *temp = new Graphene(graphene_table_size, th_RH);
        graphene[i][j] = *temp;
        graphene_statistics[i][j] = 0;
      }
    }
    cout << "[Graphene] Initialize finished...\n";
  }
  // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
  if (get_param_str("rh_prevention_scheme") == "hydra") {
    rh_mode = rh_hydra;
    hydra = new Hydra(
                            get_param_uint64("hydra_gct_threshold", 200),
                            get_param_uint64("hydra_gct_num_entries", 32768),
                            get_param_uint64("hydra_rcc_num_entries", 8192),
                            get_param_uint64("hydra_rct_threshold", 250),
                            num_ranks_per_mc * num_banks_per_rank * num_pages_per_bank,
                            page_sz_base_bit);
    num_refreshes_for_hydra = 0;
    num_rct_updates++;
    hydra_ref = 0;
    hydra_update = 0;
  }
  // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
  if (get_param_str("rh_prevention_scheme") == "srs") {
    rh_mode = rh_srs;
    rrs_threshold = get_param_uint64("rrs_threshold", 800);
    rrs_hrt_threshold = get_param_uint64("rrs_hrt_threshold", 400);
    rrs_hrt_num_entry = get_param_uint64("rrs_hrt_num_entry", 1600);
    rrs_rit_num_entry = 0;
    rrs_num_tables = 1;
    rrs_delay_rit_access = get_param_uint64("rrs_delay_rit_access", 5);
    rrs_delay_row_swap = get_param_uint64("rrs_delay_row_swap", 8192);
    rrs.insert(rrs.begin(), num_ranks_per_mc, vector<RRS>(num_banks_per_rank, RRS(
          rrs_threshold, rrs_hrt_threshold, rrs_rit_num_entry, rrs_hrt_num_entry, rrs_num_tables
    )));
    rrs_num_row_swap = 0;
    rrs_num_lazy_evictions = 0;

    // HRT
    for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        RRS *temp = 
            new RRS(rrs_threshold, rrs_hrt_threshold, rrs_rit_num_entry, 
                    rrs_hrt_num_entry, rrs_num_tables);
        rrs[i][j] = *temp;
      }
    }
  }
  // "RAMPART: RowHammer Mitigation and Repair for Sever Memory Systems," MEMSYS, 2023
  if (get_param_str("rh_prevention_scheme") == "rampart") {
    rh_mode = rh_rampart;
    cout << "[RAMPART] set RAAIMT to " << RAAIMT << "\n";
    cout << "[RAMPART] set tRFM_t to " << tRFM_t << "\n";
    cout << "[RAMPART] Initialize finished.\n";
  }
  // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
  if (get_param_str("rh_prevention_scheme") == "abacus") {
    rh_mode = rh_abacus;
    uint64_t abacus_prt = get_param_uint64("abacus_prt", 2048);  // PRT (N_RH / 2)
    uint64_t abacus_rct = get_param_uint64("abacus_rct", 2046);  // RCT (PRT - 2)
    uint64_t abacus_num_entry = get_param_uint64("abacus_num_entry", 283);  // Nentry (N_act / PRT)
    // init (per-rank abacus row table)
    cout << "[ABACuS] initialize table\n";
    abacus.resize(num_ranks_per_mc);
    for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
      ABACuS* temp = new ABACuS(abacus_num_entry, abacus_prt, abacus_rct);
      abacus[i] = temp;
    }
  }
  // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
  if (get_param_str("rh_prevention_scheme") == "prac") {
    rh_mode = rh_prac;
    cout << "[PRAC4] init: " << num_ranks_per_mc << "," << num_banks_per_rank << "," << num_pages_per_bank << "\n";
    // per-row activation counting
    prac.resize(num_ranks_per_mc);
    for (auto &rk : prac) {
      rk.resize(num_banks_per_rank);
      for (auto &bk : rk) {
        bk.resize(num_pages_per_bank);
      }
    }
  }
  /* BlockHammer implementation: BlockHammer, HPCA, 2021 */
  if (get_param_str("rh_prevention_scheme") == "blockhammer") {
    rh_mode = rh_blockhammer;
  }
  static BlockHammerParameters p;
  if (rh_mode == rh_blockhammer) {
    p.num_ranks_per_mc = num_ranks_per_mc;
    p.num_banks_per_rank = num_banks_per_rank;
    p.blistTH = get_param_uint64("th_RH",32768);
    p.cntSize = get_param_uint64("cntSize", 1024);
    p.totNumHashes = get_param_uint64("totNumHashes", 4);
    //p.historySize = get_param_uint64("historySize",0); // TODO not mdfile, but calculate Delay here.
    p.hash = new H3();
    p.attackthrottler = get_param_str("attackthrottler") == "true" ? true : false;
    p.quota_base = get_param_uint64("quota_base",2);
    p.Nth = get_param_uint64("blockhammer_Nth",139000);

    // 1150156800 == 32ms, 1755 == tRC == 48.640ns, 
    //p.Delay = float(1150156800 - (p.blistTH*1755))/float(p.Nth - p.blistTH);

    // RowArmor
    // From BlockHammer: tDelay = (tCBF - (NBL*tRC)) / ((tCBF/tREFW) * NRH - NBL)
    // tDelay = float(1022361600 - (p.blistTH * 159 * 32))
    p.Delay = float(1022361600 - (p.blistTH * 159 * 32)) / float(p.Nth - p.blistTH);
    std::cout << "Delay: " << p.Delay << std::endl;
    //p.historySize = p.Delay * 20 /1755;
    //p.historySize = p.Delay*0.027777/9;
    // p.historySize = p.Delay / tRC = p.Dleay / (tRAS + tRP) = p.Delay / 159
    p.historySize = p.Delay / 159;
    std::cout << "historySize: Delay/tRC : " << p.historySize << std::endl;
  }

  my_bh = new BlockHammer(p);
  my_bh->mc = this;
  if (bliss) {
    my_bliss = new Bliss(4, 10000 * process_interval);  // threshold and clearing interval
    geq->add_event(my_bliss->clearing_interval, this);
  }
  
  /* RowHammer attackers (Denial-of-Service) */
  use_attacker = get_param_str("use_attacker") == "true" ? true : false;
  if (use_attacker)
  {
    // parameters
    num_rowhammer_attackers = (uint32_t)get_param_uint64("num_attacker_threads", 1);
    uint32_t num_attack_rows_per_thread = (uint32_t)get_param_uint64("num_attack_rows_per_thread", 8);
    uint32_t attacker_max_req_count = (uint32_t)get_param_uint64("attacker_max_req_count", 8);
    hammer_threads.resize(num_rowhammer_attackers);
    for (size_t i = 0; i < num_rowhammer_attackers; ++i)
    {
      RowHammerGen* hammer = new RowHammerGen(num_hthreads-i-1, num_attack_rows_per_thread, attacker_max_req_count);
      hammer->num_ranks_per_mc = num_ranks_per_mc;
      hammer->num_banks_per_rank = num_banks_per_rank;
      hammer->rank_interleave_base_bit = rank_interleave_base_bit;
      hammer->bank_interleave_base_bit = bank_interleave_base_bit;
      hammer->mc_interleave_base_bit = mc_interleave_base_bit;
      hammer->page_sz_base_bit = page_sz_base_bit;
      hammer->interleave_xor_base_bit = interleave_xor_base_bit;
      hammer->num_mcs = num_mcs;
      for (int j = 0; j < num_attack_rows_per_thread; ++j) {
        // target just a single rank and a single bank per thread
        hammer->address_list.push_back(hammer->make_address(0, i, 2*j+229));  // double-sided
      }
      hammer_threads[i] = hammer;
      //hammer_threads[i] = std::make_unique<RowHammerGen>(*hammer);
    }
  }
}

RowHammerGen::RowHammerGen()
{
  // placeholder
}

RowHammerGen::RowHammerGen(uint32_t th_num, uint32_t num, uint32_t max_req_count):
    attacker_thread_number(th_num), num_attack_rows_per_thread(num),
    attacker_max_req_count(max_req_count),
    attacker_req_count(0), address_list(), curr_idx(0) {
  // placeholder
}

LocalQueueElement * RowHammerGen::make_event(Component * comp) {
  if (curr_idx >= num_attack_rows_per_thread)
  {
    curr_idx = 0; // cyclic
  }
  uint64_t address = address_list[curr_idx++];
  // event: RD
  LocalQueueElement * event = new LocalQueueElement(comp, et_read, address);
  event->th_id = attacker_thread_number;
  return event;
}

uint32_t RowHammerGen::log2(uint32_t num) {
  uint32_t log2 = 0;
  while (num > 1) {
    log2 += 1;
    num = (num >> 1);
  }
  return log2;
}

uint64_t RowHammerGen::make_address(uint32_t rank, uint32_t bank, uint64_t page) {
  uint64_t address = 0;
  uint32_t slice_bit = (bank_interleave_base_bit - 1) - mc_interleave_base_bit;
  uint32_t slice = pow(2, slice_bit);
  slice = page % slice;

  uint32_t temp = (interleave_xor_base_bit - 1) - mc_interleave_base_bit;
  temp -= log2(num_banks_per_rank);
  uint32_t interleave = page >> temp;

  uint32_t mc_bit = (0 ^ interleave) % num_mcs;
  address += mc_bit;
  address += (slice << 1); // shift one bit for mc_bit

  uint32_t bank_bit = (bank ^ interleave) % num_banks_per_rank;
  address += (bank_bit << (slice_bit + log2(num_mcs)));
  address += (page >> slice_bit) << (log2(num_banks_per_rank) + log2(num_mcs) + slice_bit);

  return address << page_sz_base_bit;
}

Bliss::Bliss(uint32_t threshold, uint32_t clearing_interval):
  threshold(threshold), clearing_interval(clearing_interval),
  th_id(0), num_req_served(0) {
    clear_blacklist();
}

void Bliss::update_blacklist(uint32_t th_id) {
  if (this->th_id == th_id) {  // consecutive request
    num_req_served++;
    if (num_req_served >= threshold) {  // blacklisting
      blacklist.insert(th_id);
      this->th_id = 0;
      num_req_served = 0;
    }
  } else {
    this->th_id = th_id;
    num_req_served = 1;
  }
}

void Bliss::clear_blacklist() {
  blacklist.clear();
}

bool Bliss::is_blacklisted(uint32_t th_id) {
  auto it = blacklist.find(th_id);
  bool is_blacklisted = (it == blacklist.end()) ? false : true;
  return is_blacklisted;
}

MemoryController::~MemoryController()
{
  if (num_read > 0)
  {
    uint64_t num_activated_bank = 0;
    for (uint32_t i = 0; i < num_ranks_per_mc; i++)
    {
      for (uint32_t j = 0; j < num_banks_per_rank + num_banks_per_rank_ab; j++)
      {
        if (bank_status[i][j].action_type == mc_bank_activate
           || bank_status[i][j].action_type == mc_bank_read || bank_status[i][j].action_type == mc_bank_write)
        {
          num_activated_bank++;
        }
      }
    }
    accu_num_activated_bank += (mcsim->get_curr_time() - last_call_process_event) / process_interval * num_activated_bank;
    cout << "  -- MC  [" << setw(3) << num << "] : (rd, wr, act, res, pre) = ("
         << setw(9) << num_read << ", " << setw(9) << num_write << ", "
         << setw(9) << num_activate << ", " << setw(9) << num_restore << ", " << setw(9) << num_precharge
         << "), # of WR->RD switch = " << num_write_to_read_switch
         << ", #_refresh = " << num_refresh << ", "
         << os_page_acc_dist.size() << " pages acc, AB (rd, wr, act) = ("
         << setw(9) << num_ab_read << ", " << setw(9) << num_ab_write << ", "
         << setw(9) << num_ab_activate << "), "
         << "avg_tick_in_mc_for_rd= " << packet_time_in_mc_acc / num_read << ", "
         << "accumulated_num_activated_bank(every mem. clock)= " << accu_num_activated_bank << endl;
    cout << "               : "
         << "l_pred (miss,hit)=( " << num_l_pred_miss + num_l_pred_miss_curr
         << ", " << num_l_pred_hit + num_l_pred_hit_curr << "), "
         << "g_pred (miss,hit)=( " << num_g_pred_miss + num_g_pred_miss_curr
         << ", " << num_g_pred_hit + num_g_pred_hit_curr << "), "
         << "o_pred (miss,hit)=( " << num_o_pred_miss + num_o_pred_miss_curr
         << ", " << num_o_pred_hit + num_o_pred_hit_curr << "), "
         << "c_pred (miss,hit)=( " << num_c_pred_miss + num_c_pred_miss_curr
         << ", " << num_c_pred_hit + num_c_pred_hit_curr << "), "
         << "t_pred (miss,hit)=( " << num_t_pred_miss    << ", " << num_t_pred_hit << "), " << endl;

    // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
    if (rh_mode == rh_para) {
      cout << "[PARA] num_refreshed_row=" << num_rh << "\n";
    }
    // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
    if (rh_mode == rh_graphene) {
      cout << "[Graphene] graphene-induced refreshes <rank, bank, act>\n";
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          cout << "<" << i << "," << j << "," << graphene_statistics[i][j] << ">\n";
        }
      }
    }

    cout << "               : "
         << "num_refreshed_row_to_prevent_row_hammering= " << num_rh << endl;
    // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
    if (rh_mode == rh_hydra) {
      cout << "[Hydra statistics]\n"
           << "# refreshes for Hydra: " << num_refreshes_for_hydra << "\n"
           << "# rct updates: " << num_rct_updates << "\n"
           << "gct, rcc, rct\n";
      cout << hydra->num_gct_access << ","  << hydra->num_rcc_access << "," << hydra->num_rct_access << "\n";
      cout << "hydra_ref, hydra_update," << hydra_ref << "," << hydra_update << "\n";
    }
    // "RAMPART: RowHammer Mitigation and Repair for Sever Memory Systems," MEMSYS, 2023
    if (rh_mode == rh_rampart) {
      cout << "[RAMPART] statistics\n";
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          cout << "[" << i << ", " << j << "] " << num_RFM[i][j] << "\n";
        }
      }
    }
    // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
    if (rh_mode == rh_abacus) {
      cout << "[ABACuS statistics]\n";
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        abacus[i]->debug(i);
      }
    }
    // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
    if (rh_mode == rh_prac)  {
      cout << "[PRAC4] statistics\n";
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          cout << "[" << i << ", " << j << "] " << num_RFM[i][j] << "\n";
        }
      }
    }

    // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
    if (rh_mode == rh_srs) {
      cout << "[SRS] statistics\n";
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          cout << "[" << i << ", " << j << "]" << endl;
          rrs[i][j].debug();  // print stat
        }
      }
      cout << "Triggered # of swaps: " << rrs_num_row_swap << "\n"
           << "# of lazy evictions: " << rrs_num_lazy_evictions << "\n";
    }
  }

  /* RowHammer attacks (for Denial of Service) */
  if (use_attacker) {
    for (uint32_t i = 0; i < num_rowhammer_attackers; ++i) {
      delete hammer_threads[i];
    }
    hammer_threads.clear();
  }

  /* RowHammer statistics */
  std::cout << "[Thread statistics] th_id : access, activate\n";
  for (uint32_t i = 0; i < num_hthreads; ++i) {
    if (acc_from_a_th[i] != 0 || act_from_a_th[i] != 0) {
      std::cout << i << " : " << acc_from_a_th[i] << ", " << act_from_a_th[i] << std::endl;
    }
  }

  if (display_os_page_usage == true)
  {
    for(map<uint64_t, uint64_t>::iterator iter = os_page_acc_dist.begin(); iter != os_page_acc_dist.end(); ++iter)
    {
      cout << "  -- page 0x" << setfill('0') << setw(8) << hex << iter->first * (1 << page_sz_base_bit)
           << setfill(' ') << dec << " is accessed (" 
           << setw(7) << mcsim->os_page_req_dist[iter->first]
           << ", " << setw(7) << iter->second << ") times at (Core, MC)." <<  endl;
    }
  }
}

void MemoryController::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time += process_interval - event_time%process_interval;
  }

  if (is_fixed_latency == true)
  {
    if (local_event->type == et_evict ||
        local_event->type == et_evict_owned ||
        local_event->type == et_dir_evict)
    {
      num_write++;
      delete local_event;
    }
    else
    {
      num_read++;
      packet_time_in_mc_acc += mc_to_dir_t;
      if (from->type == ct_memory_controller)
      {
        delete local_event;
      }
      else
      {
        if (is_shared_llc) crossbar->add_mcrp_event(event_time + mc_to_dir_t, local_event);
        else directory->add_rep_event(event_time + mc_to_dir_t, local_event);
      }
    }
  }
  else if (is_fixed_bw_n_latency == true)
  {
    last_process_time = (event_time == 0 || event_time > last_process_time) ? event_time : (last_process_time + process_interval);
    //if (num == 0) {cout << "[" << setw(10) << event_time << "][" << setw(10) << last_process_time << "] " << setw(3) << num << "  "; local_event->display();}

    if (local_event->type == et_evict ||
        local_event->type == et_evict_owned ||
        local_event->type == et_dir_evict)
    {
      num_write++;
      delete local_event;
    }
    else
    {
      num_read++;
      packet_time_in_mc_acc += mc_to_dir_t + (last_process_time - event_time);
      if (from->type == ct_memory_controller)
      {
        delete local_event;
      }
      else
      {
        if (is_shared_llc) crossbar->add_mcrp_event(event_time + mc_to_dir_t, local_event);
        else directory->add_rep_event(last_process_time + mc_to_dir_t, local_event);
      }
    }
  }
  else
  {
    geq->add_event(event_time, this);
    req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
    event_n_time.insert(pair<LocalQueueElement *, uint64_t>(local_event, event_time));
  }

  // update access distribution
  num_reqs++;
  uint64_t page_num = (local_event->address >> page_sz_base_bit);
//  map<uint64_t, uint64_t>::iterator p_iter = os_page_acc_dist_curr.find(page_num);
//
//  if (p_iter != os_page_acc_dist_curr.end())
//  {
//    (p_iter->second)++;
//  }
//  else
//  {
//    os_page_acc_dist_curr.insert(pair<uint64_t, uint64_t>(page_num, 1));
//  }
}

void MemoryController::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  add_req_event(event_time, local_event, from);
}


uint64_t MemoryController::get_page_num(uint64_t addr)
{
  uint64_t page_num = addr;

  page_num = (((page_num >> base0) / width0) << base0) + (page_num % (1 << base0));
  page_num = (((page_num >> base1) / width1) << base1) + (page_num % (1 << base1));
  page_num = (((page_num >> base2) / width2) << base2) + (page_num % (1 << base2));

  return (page_num >> page_sz_base_bit);
}

uint64_t MemoryController::get_col_num(uint64_t addr)
{
  uint64_t col_num = addr;

  col_num = (((col_num >> base0) / width0) << base0) + (col_num % (1 << base0));
  col_num = (((col_num >> base1) / width1) << base1) + (col_num % (1 << base1));
  col_num = (((col_num >> base2) / width2) << base2) + (col_num % (1 << base2));

  return (col_num %(1 << page_sz_base_bit));
}

inline void MemoryController::show_page_acc_pattern(
    uint64_t th_id, 
    uint32_t rank, 
    uint32_t bank, 
    uint64_t page, 
    mc_bank_action type, 
    uint64_t curr_time)
{
  cout<<"  -- (curr_time, th, mc, rank, bank, page, cmd) = (" << dec << curr_time<< ", " << th_id << ", " 
      << num << ", " << rank << ", " << bank << ", 0x" << hex << page << ", " << type
      << ")" << dec << endl;
}

void MemoryController::show_state(uint64_t curr_time)
{
  cout << "  -- MC  [" << num << "] : curr_time = " << curr_time
       << ", " << policy << ", " << curr_tournament_idx << endl;

  uint32_t i;
  vector<LocalQueueElement *>::iterator iter;
  for (iter = req_l.begin(), i = 0; iter != req_l.end()/* && i < req_window_sz*/ ; ++iter, ++i)
  {
    uint64_t address  = (*iter)->address;
    uint32_t rank_num = get_rank_num(address);
    uint32_t bank_num = get_bank_num(address, (*iter)->th_id);
    cout << "  -- req_l[" << setw(2) <<  i << "] = (" << rank_num << ", " << bank_num
         << ", 0x" << hex << get_page_num((*iter)->address) << dec << "): "
         << hex << (uint64_t *)(*iter) << dec << " : "; (*iter)->display();
  }
  if (curr_batch_last >= 0)
  {
    cout << "  -- current batch ends at req_l[" << setw(2) << curr_batch_last << "]" << endl;
  }

  for (uint32_t i = 0; i < num_ranks_per_mc; i++)
  {
    for(uint32_t k = 0; k < num_banks_per_rank + num_banks_per_rank_ab; k++)
    {
      cout << "  -- bank_status[" << setw(2) <<  i << "][" << setw(2) << k << "] = ("
        << setw(10) << bank_status[i][k].action_time << ", "
        << hex << "0x" << bank_status[i][k].page_num << dec << ", "
        << bank_status[i][k].action_type << "), "
        << bank_status[i][k].action_type_prev << endl;
    }
  }


  for (uint32_t i = 0; i < num_ranks_per_mc; i++)
  {
    cout << "  -- last_activate_time[" << i << "] = " << last_activate_time[i] << endl;
  }


  map<uint64_t, mc_bank_action>::iterator miter = rd_dp_status.begin();

  while (miter != rd_dp_status.end())
  {
    cout << "  -- rd_dp_status = ("
      << miter->first << ", "
      << miter->second << ")" << endl;
    ++miter;
  }
  miter = wr_dp_status.begin();

  while (miter != wr_dp_status.end())
  {
    cout << "  -- wr_dp_status = ("
      << miter->first << ", "
      << miter->second << ")" << endl;
    ++miter;
  }
}


uint32_t MemoryController::process_event(uint64_t curr_time)
{
  // for the standby current calculation considering IDD2N instead of IDD3N 
  uint64_t num_activated_bank = 0;
  for (uint32_t i = 0; i < num_ranks_per_mc; i++)
  {
    for (uint32_t j = 0; j < num_banks_per_rank + num_banks_per_rank_ab; j++)
    {
      if (bank_status[i][j].action_type == mc_bank_activate
      || bank_status[i][j].action_type == mc_bank_read || bank_status[i][j].action_type == mc_bank_write)
      {
        num_activated_bank++;
      }
    }
  }
  accu_num_activated_bank += (curr_time - last_call_process_event) / process_interval * num_activated_bank;
  last_call_process_event = curr_time;

  if (last_process_time > 0 && (is_fixed_latency == true || is_fixed_bw_n_latency == true))
  {
    packet_time_in_mc_acc += (curr_time - last_process_time) * req_l.size();
  }
  last_process_time = curr_time;

  if (bliss && curr_time % (my_bliss->clearing_interval) == 0 && req_l.size() != 0) {
    my_bliss->clear_blacklist();
    geq->add_event(curr_time + my_bliss->clearing_interval, this);
  }

  pre_processing(curr_time);

  uint32_t i, i2;
  vector<LocalQueueElement *>::iterator iter, iter2;

  // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
  if (rh_mode == rh_srs) {
    bool flag = false;
    uint32_t rn = 0;

    // check the row_swap flag
    for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        BankStatus &curr_bank = bank_status[i][j];
        if (curr_bank.rh_swap == true) {
          // flag set to curr_bank. Broadcast to rank.
          flag = true;
          rn = i;
          curr_bank.rh_swap = false;
        }
      }
      if (flag) {
        break;
      }
    }

    if (flag) { 
      // trigger swap for all ranks, banks in MC
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        BankStatus &curr_bank = bank_status[rn][j];
        curr_bank.action_type = mc_bank_precharge;
        curr_bank.action_type_prev = mc_bank_refresh;
        // PRE the current row and two row swaps
        curr_bank.action_time = curr_time + ((tRAS + tRP + rrs_delay_row_swap * 2) * process_interval);
      }
    }
  }
  // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
  if (rh_mode == rh_abacus) {
    // checking refresh cycles (rh_update flag)
    bool refresh_cycle_flag = false;
    uint32_t target_rankn = 0;

    // check each bank
    for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        BankStatus &curr_bank = bank_status[i][j];
        if (curr_bank.rh_update == true) {
          refresh_cycle_flag = true;
          target_rankn = i;
          curr_bank.rh_update = false;
        }
      }
      if (refresh_cycle_flag) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          BankStatus &curr_bank = bank_status[i][j];
          curr_bank.rh_update = false;
        }
        break;
      }
    }

    // refresh_cycle (refresh all rows)
    if (refresh_cycle_flag) {
      abacus[target_rankn]->refresh_cycle();
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        BankStatus &curr_bank = bank_status[target_rankn][j];
        curr_bank.action_type = mc_bank_precharge;
        curr_bank.action_type_prev = mc_bank_refresh;
        curr_bank.action_time = curr_time + (tRAS + tRP) * process_interval + tRFC_t * 8192; // ???
      }
    }

    // checking preventive refreshes
    bool preventive_refresh_flag = false;
    uint32_t rankn = 0;

    // Check preventive refresh flag
    // We reuse rh_swap flag for preventive refresh flag in ABACuS
    for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        BankStatus &curr_bank = bank_status[i][j];
        if (curr_bank.rh_swap == true) {
          preventive_refresh_flag = true;
          rankn = i;
          curr_bank.rh_swap = false;
        }
      }

      if (preventive_refresh_flag) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          BankStatus &curr_bank = bank_status[i][j];
          curr_bank.rh_swap = false;
        }
        break;
      }
    }

    if (preventive_refresh_flag) {
      abacus[rankn]->preventive_refresh();
      for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
        BankStatus &curr_bank = bank_status[rankn][j];
        curr_bank.action_type = mc_bank_precharge;
        curr_bank.action_type_prev = mc_bank_refresh;
        curr_bank.action_time = curr_time +  2 * (tRAS + tRP) * process_interval * blast_radius;
      }
    }
  }

  // auto-refresh
  if (refresh_interval != 0 && curr_time % (refresh_interval / num_ranks_per_mc) == 0)
  {
    geq->add_event(curr_time + refresh_interval/num_ranks_per_mc, this);  // add next event
    geq->add_event(curr_time + process_interval, this);
    num_refresh++;
    curr_refresh_rank = (curr_refresh_rank + 1) % num_ranks_per_mc;
    curr_refresh_page = (curr_refresh_page + ((curr_refresh_rank == 0) ? (num_pages_per_bank / 8192) : 0)) % num_pages_per_bank;

    for (uint32_t j = 0; j < num_banks_per_rank; j++)
    {
      BankStatus & curr_bank = bank_status[curr_refresh_rank][j];
      if (display_page_acc_pattern == true && curr_bank.action_type == mc_bank_precharge 
          && curr_bank.action_type_prev != mc_bank_refresh && curr_bank.action_time < curr_time) 
      { //print cmd when bank is precharged in closed policy 
        show_page_acc_pattern(curr_bank.th_id, curr_refresh_rank, j, curr_bank.page_num, mc_bank_precharge, curr_bank.action_time);
      }
      curr_bank.action_type       = mc_bank_precharge;
      curr_bank.action_type_prev  = mc_bank_refresh;
      curr_bank.action_time  = curr_time + tRFC_t - tRP*process_interval;
      curr_bank.page_num     = curr_refresh_page;
      curr_bank.skip_pred    = true;

      if (display_page_acc_pattern == true)
      {
        show_page_acc_pattern(10000, curr_refresh_rank, j, curr_refresh_page, mc_bank_refresh, curr_time);
      }

      // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
      if (rh_mode == rh_graphene) {
        // auto-refresh
        uint64_t num_rows_per_refresh = num_pages_per_bank / 8192;  
        for (vector<uint32_t>::size_type i = 0; i < graphene_flush_time.size(); ++i) {
          if ((curr_refresh_page / num_rows_per_refresh) == graphene_flush_time[i]) {
            // refresh corresponding rows (pages)
            graphene[curr_refresh_rank][j].flush();
          }
        }
      }
      // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
      if (rh_mode == rh_hydra) { 
        // rank-level periodic reset (auto-refresh)
        if (curr_refresh_page == 0) {
          hydra->reset();
        }
      }
      // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
      if (rh_mode == rh_srs) {
        if (curr_refresh_page == 0) { 
          // rank-level periodic reset (auto-refresh)
          rrs[curr_refresh_rank][j].periodic_refresh();
        }
      }
      // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
      if (rh_mode == rh_abacus) {
        // auto-refresh of tREFW interval
        uint64_t num_rows_per_refresh = num_pages_per_bank / 8192;
        if ((curr_refresh_page / num_rows_per_refresh) == 0) {
          // tREFW interval
          abacus[curr_refresh_rank]->periodic_refresh();
        }
      }
      // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
      if (rh_mode == rh_prac) {
        // auto-refresh
        uint64_t num_rows_per_refresh = num_pages_per_bank / 8192;
        for (uint64_t i = curr_refresh_page; i < (curr_refresh_page + num_rows_per_refresh); ++i) {
          prac[curr_refresh_rank][j][i] = 0;
        }
      }
      // "BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows," HPCA, 2021
      if (rh_mode == rh_blockhammer) {
        if (curr_refresh_page == 0 || curr_refresh_page == 4096) { // at every tREFW/2
          uint32_t valid_CBF = (curr_refresh_page == 0) ? 0 : 1;
          if (valid_CBF == 0) {
            my_bh->rowblocker_CBFs[curr_refresh_rank][j].first->validate(true);
            my_bh->rowblocker_CBFs[curr_refresh_rank][j].second->validate(false);
            my_bh->rowblocker_CBFs[curr_refresh_rank][j].second->reset();
            if (my_bh->attackthrottler == true) {
              my_bh->attackthrottlers[curr_refresh_rank][j].first->validate(true);
              my_bh->attackthrottlers[curr_refresh_rank][j].second->validate(false);
              my_bh->attackthrottlers[curr_refresh_rank][j].second->print_AttackThrottler();
              my_bh->attackthrottlers[curr_refresh_rank][j].second->reset();
            }
          }
          else if (valid_CBF == 1) {
            my_bh->rowblocker_CBFs[curr_refresh_rank][j].second->validate(true);
            my_bh->rowblocker_CBFs[curr_refresh_rank][j].first->validate(false);
            my_bh->rowblocker_CBFs[curr_refresh_rank][j].first->reset();
            if (my_bh->attackthrottler == true) {
              my_bh->attackthrottlers[curr_refresh_rank][j].second->validate(true);
              my_bh->attackthrottlers[curr_refresh_rank][j].first->validate(false);
              my_bh->attackthrottlers[curr_refresh_rank][j].first->print_AttackThrottler();
              my_bh->attackthrottlers[curr_refresh_rank][j].first->reset();
            }
          }
        }
      }
    }
    return 0;
  }

  // Check RFM (Refresh management)
  if (RAAIMT != 0) {
    // "RAMPART: RowHammer Mitigation and Repair for Sever Memory Systems," MEMSYS, 2023
    if (rh_mode == rh_rampart) {
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          if (todo_RFM_flag[i][j]) {
            todo_RFM_flag[i][j] = false;
            BankStatus &curr_bank = bank_status[i][j];
            curr_bank.action_type = mc_bank_precharge;
            curr_bank.action_time = curr_time + tRFM_t; // per-bank RFM
            RAA_counter[i][j] -= RAAIMT;
            num_RFM[i][j]++;
          }
        }
      }
    }
    // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
    if (rh_mode == rh_prac) {
      // Check RFM flag for each bank
      for (uint32_t i = 0; i < num_ranks_per_mc; ++i) {
        for (uint32_t j = 0; j < num_banks_per_rank; ++j) {
          if (todo_RFM_flag[i][j]) {
            todo_RFM_flag[i][j] = false;
            BankStatus &curr_bank = bank_status[i][j];
            curr_bank.action_type = mc_bank_precharge;
            curr_bank.action_time = curr_time + tRFM_t; // per-bank RFM
            num_RFM[i][j]++;
          }
        }
      }
    }
  }

  if ((policy == mc_sched_m_open || policy == mc_sched_a_open) && req_l.empty() == true) {
    // minimalist open need to check a bank unless it is already precharged
    for (uint32_t i = 0; i < num_ranks_per_mc; i++) {
      for(uint32_t k = 0; k < num_banks_per_rank + num_banks_per_rank_ab; k++) {
        BankStatus & curr_bank = bank_status[i][k];
        if (curr_bank.action_type != mc_bank_precharge && curr_bank.action_type != mc_bank_idle) {
          i = num_ranks_per_mc;
          geq->add_event(curr_time + process_interval, this);
          break;
        }
      }
    }
  }

  int32_t c_idx    = -1;                                       // candidate index
  vector<LocalQueueElement *>::iterator c_iter = req_l.end();
  bool    page_hit = false;
  bool blacklisted = true;
  int32_t num_req_from_the_same_thread = req_l.size() + 1;

  for (iter = req_l.begin(), i = 0; iter != req_l.end() && i < req_window_sz;)
  {
    if (bliss) {
      if (c_idx >= 0 && page_hit && blacklisted == false) {
        break;
      }
    }
    else if (c_idx >= 0 && iter != req_l.begin() && (int32_t)i > curr_batch_last) {
      // we found a candidate from the ready batch already
      break;
    }

    // check constraints
    uint64_t address  = (*iter)->address;
    event_type type   = (*iter)->type;
    uint64_t th_id    = (*iter)->th_id;
    uint32_t rank_num = get_rank_num(address);
    uint32_t bank_num = get_bank_num(address, th_id);
    uint64_t page_num = get_page_num(address);
    uint32_t bank_group_num = bank_num % num_bank_groups;

    bool access_agile = (address >> 63 != 0 ||  // this is an old agreement between McSim and main.cc
                         (bank_num < num_banks_with_agile_row &&
                          reciprocal_of_agile_row_portion != 0 &&
                          page_num%reciprocal_of_agile_row_portion == 0));
    uint32_t tRCD_curr = (access_agile) ? tRCD_ab : tRCD;
    uint32_t tRAS_curr = (access_agile) ? tRAS_ab : tRAS;
    uint32_t tRP_curr  = (access_agile) ? tRP_ab  : tRP;
    uint32_t tBL_curr  = (access_agile) ? tBL_ab  : tBL;
    uint32_t tRP_prev  = (last_time_from_ab[rank_num][bank_num]) ? tRP_ab  : tRP;
    uint32_t tRES_prev = (last_time_from_ab[rank_num][bank_num]) ? (tRAS_ab - tRCD_ab) : (tRAS - tRCD);  // restore

    BankStatus & curr_bank = bank_status[rank_num][bank_num];
    map<uint64_t, mc_bank_action>::iterator miter, rd_miter, wr_miter;
    map<uint64_t, uint32_t>::iterator bgiter;  // bank group related iterator

    if (curr_bank.action_type != mc_bank_precharge && curr_bank.action_time > curr_time)
    {
      continue;
    }
    switch (curr_bank.action_type)
    {
      case mc_bank_precharge:  // actually there are two types of precharge governed by tRP or tRES+tRP
        if ((cont_restore_after_write == false &&
             curr_bank.latest_write_time > curr_bank.latest_activate_time &&
             curr_bank.action_time + (tRP_prev+tRES_prev)*process_interval > curr_time) ||
            (cont_restore_after_activate == false &&
             curr_bank.latest_write_time < curr_bank.latest_activate_time &&
             curr_bank.action_time + (tRP_prev+tRES_prev)*process_interval > curr_time) ||
            (curr_bank.action_time + tRP_prev*process_interval > curr_time))
        {
          // tRP
          break;
        }
        if (curr_time < curr_bank.latest_activate_time +
                        (last_time_from_ab[rank_num][bank_num] ? (tRAS_ab+tRP_ab) : (tRAS+tRP))*process_interval)
        {
          // not yet tRC
          break;
        }

      case mc_bank_idle:
        if (bliss) {
          if (last_activate_time[rank_num] + tRR*process_interval <= curr_time &&
            (c_idx == -1 || (blacklisted && my_bliss->is_blacklisted((*iter)->th_id) == false))) {

            // BlockHammer's rowblocker
            if (rh_mode == rh_blockhammer) {
              // Query rowblocker - only this c_idx results in a Activation.
              if (my_bh->query_rowblocker(page_num, rank_num, bank_num, th_id, curr_time) == page_num) {
                my_bh->rowblocker_throttled++;
                break; 
              }
            }

            // non-blacklisted thread has higher priority than blacklisted thread
            c_idx = i;
            c_iter = iter;
            blacklisted = my_bliss->is_blacklisted((*iter)->th_id);
            page_hit = false;
          }
        }
        else if (page_hit == false &&  // page_hit has priority 2
            last_activate_time[rank_num] + tRR*process_interval <= curr_time)
        {
          // PARBS
          if ((num_req_from_a_th[th_id] < num_req_from_the_same_thread) ||
              (num_req_from_a_th[th_id] == num_req_from_the_same_thread && c_idx == -1))
          {
            if (display_page_acc_pattern == true && curr_bank.action_time > 0)
            {
              show_page_acc_pattern(th_id, rank_num, bank_num, curr_bank.page_num, mc_bank_precharge, curr_bank.action_time);
            }

            // BlockHammer's rowblocker
            if (rh_mode == rh_blockhammer) {
              // Query rowblocker - only this c_idx results in a Activation.
              if (my_bh->query_rowblocker(page_num, rank_num, bank_num, th_id, curr_time) == page_num) {
                my_bh->rowblocker_throttled++;
                break; 
              }
            }

            c_idx  = i;
            c_iter = iter;
            num_req_from_the_same_thread = num_req_from_a_th[th_id];
          }
        }
        break;
      case mc_bank_activate:
        if (curr_bank.page_num != page_num || curr_bank.action_time + tRCD_curr*process_interval > curr_time)
        {
          break;
        }
      case mc_bank_write:
        if (curr_bank.action_time + tBBLW*process_interval > curr_time)
        { // differentiate the delay for RD from WR
          break;
        }
      case mc_bank_read:
        if ((curr_bank.action_type == mc_bank_write) && (curr_bank.action_time + tBBLW*process_interval > curr_time))
        { // differentiate the delay for WR from RD
          break;
        }
        if (curr_bank.action_time + tBBL*process_interval > curr_time)
        {
        }
        else if (curr_bank.page_num != page_num)
        { // row miss
          if ((curr_bank.action_type == mc_bank_read  && (curr_bank.action_time+tRTP*process_interval > curr_time)) ||
              (curr_bank.action_type == mc_bank_write && (curr_bank.action_time+tWTP*process_interval > curr_time)))
          { // tRTP or tWTP
            break;
          }
          if (curr_bank.latest_activate_time > curr_bank.latest_write_time)
          { // only reads after activate
            if (cont_precharge_after_activate == true &&
                curr_bank.latest_activate_time + (tRAS_curr+tRP_curr)*process_interval > curr_time)
            {  // tRAS+tRP
              break;
            }
            if (cont_restore_after_activate == true &&
                curr_bank.latest_activate_time + tRAS_curr*process_interval > curr_time)
            {  // tRAS
              break;
            }
          }
          else
          { // there is at least one write after activate
            if (cont_precharge_after_write == true &&
                curr_bank.latest_activate_time + (tRAS_curr+tRP_curr)*process_interval > curr_time)
            {  // tRAS+tRP
              break;
            }
            if (cont_restore_after_write == true &&
                curr_bank.latest_activate_time + tRAS_curr*process_interval > curr_time)
            {  // tRAS
              break;
            }
          }

          if (bliss) {
            if (last_activate_time[rank_num] + tRR*process_interval <= curr_time &&
              (c_idx == -1 || 
                (blacklisted && my_bliss->is_blacklisted((*iter)->th_id) == false))) {
              c_idx = i;
              c_iter = iter;
              blacklisted = my_bliss->is_blacklisted((*iter)->th_id);
              page_hit = false;
            }
          } else {
            uint32_t k = 0;
            bool need_precharge = true;
            iter2 = req_l.begin();
            while (iter2 != req_l.end() && k++ < req_window_sz)
            {
              if ((int32_t)i <= curr_batch_last && (int32_t)k > curr_batch_last + 1)
              { // PAR-BS specific constraint
                break;
              }
              if (iter == iter2)
              {
                iter2++;
                continue;
              }
              uint64_t address  = (*iter2)->address;
              if (rank_num == get_rank_num(address) &&
                  bank_num == get_bank_num(address, (*iter2)->th_id) &&
                  curr_bank.page_num == get_page_num(address))
              {
                need_precharge = false;
                break;
              }
              iter2++;
            }

            if (need_precharge == true &&
                page_hit       == false &&  // page_hit has priority 2 -- PAR-BS
                last_activate_time[rank_num] + tRR*process_interval <= curr_time)
            {
              // BlockHammer, Par-BS, BLISS
              if ((num_req_from_a_th[th_id] < num_req_from_the_same_thread) ||
                  (num_req_from_a_th[th_id] == num_req_from_the_same_thread && c_idx == -1))
              {
                c_idx  = i;
                c_iter = iter;
                num_req_from_the_same_thread = num_req_from_a_th[th_id];
              }
            }
          }
        }
        else
        { // row hit
          rd_dp_status.erase(rd_dp_status.begin(), rd_dp_status.lower_bound(curr_time));
          rd_miter         = rd_dp_status.lower_bound(curr_time + tCL*process_interval);
          wr_dp_status.erase(wr_dp_status.begin(), wr_dp_status.lower_bound(curr_time));
          wr_miter         = wr_dp_status.lower_bound(curr_time + tCL*process_interval);
          bool met_constraints = false;
          switch (type)
          {
            case et_rd_dir_info_req:
            case et_rd_dir_info_rep:
            case et_read:
            case et_e_rd:
            case et_s_rd:
              // read
              if (rd_miter == rd_dp_status.end() || rd_miter->first >= curr_time + (tCL+tBL_curr)*process_interval)
              {
                bool wrbub = false;
                // WRBUB
                wr_miter = wr_dp_status.lower_bound(curr_time + (tCL-tWRBUB)*process_interval);
                while (full_duplex == false &&
                       wr_miter != wr_dp_status.end() &&
                       wr_miter->first <= curr_time + tCL*process_interval)
                {
                  if (wr_miter->second == mc_bank_write)
                  {
                    wrbub = true;
                    break;
                  }
                  ++wr_miter;
                }

                if (wrbub == false && tWTR > 0 && last_write_time[rank_num] + tWTR*process_interval > curr_time)
                {  // tWTR constraint
                  wrbub = true;
                }

                if (wrbub == false && last_read_time.first != rank_num &&
                    curr_time < tRRBUB*process_interval + last_read_time.second)
                {  // tRRBUB
                  wrbub = true;
                }

                // rank-to-rank switching constraints
                if(wrbub == false && last_rank_num != (uint32_t)-1 && last_rank_num != rank_num)
                {
                  if(curr_time < (last_rank_acc_time + tRTRS*process_interval))
                  {
                    wrbub = true;
                  }
                }

                // bank group related constraints
                if (use_bank_group == true)
                {
                  bank_group_status.erase(bank_group_status.begin(), bank_group_status.lower_bound(curr_time - max_BUB_distance*process_interval));
                  bgiter = bank_group_status.lower_bound(curr_time - tRDBUB_same_group*process_interval);
                  while (bgiter != bank_group_status.end() && bgiter->first < curr_time)
                  {
                    uint32_t curr_rank_num = bgiter->second / num_bank_groups;
                    uint32_t curr_bank_group_num = bgiter->second % num_bank_groups;

                    if (curr_rank_num == rank_num && curr_bank_group_num == bank_group_num)
                    {
                      wrbub = true;
                      break;
                    }
                    ++bgiter;
                  }
                }

                if (wrbub == false)
                { // service the read request
                  met_constraints = true;
                }
              }
              break;
            case et_evict:
            case et_evict_owned:
            case et_dir_evict:
            case et_s_rd_wr:
              // write
              if (wr_miter == wr_dp_status.end() || wr_miter->first >= curr_time + (tCL+tBL_curr)*process_interval)
              {
                bool rwbub = false;
                // RWBUB
                rd_miter = rd_dp_status.lower_bound(curr_time + (tCL-tRWBUB)*process_interval);
                while (full_duplex == false &&
                       rd_miter != rd_dp_status.end() &&
                       rd_miter->first <= curr_time + tCL*process_interval)
                {
                  if (rd_miter->second == mc_bank_read)
                  {
                    rwbub = true;
                    break;
                  }
                  ++rd_miter;
                }

                if (rwbub == false && last_read_time_rank[rank_num] + tRTW*process_interval > curr_time)
                {  // tRTW constraint
                  rwbub = true;
                }

                // rank-to-rank switching constraints
                if(rwbub == false && last_rank_num != (uint32_t)-1 && last_rank_num != rank_num)
                {
                  if(curr_time < (last_rank_acc_time + tRTRS*process_interval))
                  {
                    rwbub = true;
                  }
                }

                // bank group related constraints
                if (use_bank_group == true)
                {
                  bank_group_status.erase(bank_group_status.begin(), bank_group_status.lower_bound(curr_time - max_BUB_distance*process_interval));
                  bgiter = bank_group_status.lower_bound(curr_time - tWRBUB_same_group*process_interval);
                  while (bgiter != bank_group_status.end() && bgiter->first < curr_time)
                  {
                    uint32_t curr_rank_num = bgiter->second / num_bank_groups;
                    uint32_t curr_bank_group_num = bgiter->second % num_bank_groups;

                    if (curr_rank_num == rank_num && curr_bank_group_num == bank_group_num)
                    {
                      rwbub = true;
                      break;
                    }
                    ++bgiter;
                  }
                }

                if (rwbub == false)
                { // service the write request
                  met_constraints = true;
                }
              }
              break;
            default:
              cout << "action_type = " << curr_bank.action_type << endl;
              show_state(curr_time);  ASSERTX(0);
              break;
          }

          // blockhammer, parbs, bliss
          if (bliss) {
            if (met_constraints == true &&
              ((my_bliss->is_blacklisted((*iter)->th_id) == false) ||  // priority 1
               (blacklisted && page_hit == false))) { // priority 3
             c_idx  = i;
             c_iter = iter;
             page_hit = true;
             blacklisted = my_bliss->is_blacklisted((*iter)->th_id);
           }
          }
          else if (met_constraints == true &&
              (page_hit == false ||  // this request is page_hit
               num_req_from_a_th[(*iter)->th_id] < num_req_from_the_same_thread))
          {
            c_idx  = i;
            c_iter = iter;
            page_hit = true;
            num_req_from_the_same_thread = num_req_from_a_th[(*iter)->th_id];
            break;
          }
        }

        break;
      default:
        cout << "curr (rank, bank) = (" << rank_num << ", " << bank_num << ")" << endl;
        show_state(curr_time);  ASSERTX(0);
        break;
    }
    ++iter;
    ++i;
  }

  if (c_idx >= 0)
  {
    i    = c_idx;
    iter = c_iter;
    // check bank_status
    uint64_t address  = (*iter)->address;
    event_type type   = (*iter)->type;
    uint64_t th_id    = (*iter)->th_id;
    uint32_t rank_num = get_rank_num(address);
    uint32_t bank_num = get_bank_num(address, th_id);
    uint64_t page_num = get_page_num(address);
    uint32_t bank_group_num = bank_num % num_bank_groups;
    BankStatus & curr_bank = bank_status[rank_num][bank_num];

    bool access_agile = (address >> 63 != 0 ||
                         (bank_num < num_banks_with_agile_row &&
                          reciprocal_of_agile_row_portion != 0 &&
                          page_num%reciprocal_of_agile_row_portion == 0));
    uint32_t mc_to_dir_t_curr = (access_agile) ? mc_to_dir_t_ab : mc_to_dir_t;
    uint32_t tRAS_curr        = (access_agile) ? tRAS_ab : tRAS;
    uint32_t tRP_curr         = (access_agile) ? tRP_ab  : tRP;
    uint32_t tBL_curr         = (access_agile) ? tBL_ab  : tBL;
    uint32_t tCL_curr         = (access_agile) ? tCL_ab  : tCL;
    uint32_t tRCD_curr        = (access_agile) ? tRCD_ab : tRCD;
    uint32_t tRES_curr        = (access_agile) ? (tRAS_ab - tRCD_ab) : (tRAS - tRCD);  // restore
    map<uint64_t, mc_bank_action>::iterator miter, rd_miter, wr_miter;
    map<uint64_t, uint32_t>::iterator bgiter;  // bank group related iterator
    mc_bank_action action_type_prev = curr_bank.action_type;

    bool ecc_bub = false;
    // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
    if (rh_mode == rh_srs) {
      // RIT table access latency
      tRCD_curr += rrs_delay_rit_access;
    }

    switch (curr_bank.action_type)
    {
      case mc_bank_precharge:
        if (policy == mc_sched_a_open && a_open_fixed_to == false)
        {
          if (curr_bank.page_num == page_num) a_open_ppc++; // this means that the last precharge was a 'premature page close'.
        }

      case mc_bank_idle:
        if (tournament_interval > 0 && curr_bank.skip_pred == false)
        { // update prediction history
          if (curr_bank.page_num == page_num)
          {
            num_l_pred_hit_curr  += (curr_bank.local_bimodal_entry < 2) ? 1 : 0;
            num_l_pred_miss_curr += (curr_bank.local_bimodal_entry > 1) ? 1 : 0;
            num_g_pred_hit_curr  += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] < 2) ? 1 : 0;
            num_g_pred_miss_curr += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] > 1) ? 1 : 0;
            num_o_pred_hit_curr++;
            num_c_pred_miss_curr++;
            curr_bank.local_bimodal_entry = (curr_bank.local_bimodal_entry == 3) ? 2 : 0;
            global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] = 
              (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] == 3) ? 2 : 0;
          }
          else
          {
            num_l_pred_miss_curr += (curr_bank.local_bimodal_entry < 2) ? 1 : 0;
            num_l_pred_hit_curr  += (curr_bank.local_bimodal_entry > 1) ? 1 : 0;
            num_g_pred_miss_curr += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] < 2) ? 1 : 0;
            num_g_pred_hit_curr  += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] > 1) ? 1 : 0;
            num_o_pred_miss_curr++;
            num_c_pred_hit_curr++;
            curr_bank.local_bimodal_entry = (curr_bank.local_bimodal_entry == 0) ? 1 : 3;
            global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] = 
              (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] == 0) ? 1 : 3;
          }
        }
        if (display_page_acc_pattern == true)
        {
          show_page_acc_pattern(th_id, rank_num, bank_num, page_num, mc_bank_activate, curr_time);
        }
        curr_bank.skip_pred   = false;
        curr_bank.action_time = curr_time;
        curr_bank.page_num    = page_num;
        curr_bank.th_id       = th_id;
        curr_bank.action_type = mc_bank_activate;
        last_activate_time[rank_num] = curr_time;
        curr_bank.latest_activate_time = curr_time;
        num_activate++;
        act_from_a_th[th_id]++;

        /* RowHammer mitigations */

        // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
    	  if (rh_mode == rh_para) {
  	      float rand_num = float(rand()) / RAND_MAX;
	        if (p_para > rand_num) {  // with probability of p_para
          	curr_bank.rh_ref = true;
            num_rh += 1;
      	  }
    	  }
        // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
        if (rh_mode == rh_graphene) {
          uint32_t result_graphene = graphene[rank_num][bank_num].activate(page_num);
          if (result_graphene) {
            num_rh += (blast_radius * 2);
            curr_bank.rh_ref = true;
            graphene_statistics[rank_num][bank_num] += (blast_radius * 2);
          }
        }
        // "BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows," HPCA, 2021
        if (rh_mode == rh_blockhammer) {
          my_bh->ACT(page_num, rank_num, bank_num, th_id, curr_time);
        }
        // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
        if (rh_mode == rh_hydra) {
          int retval = hydra->activate(curr_time, address, 0);
          if (retval == 0 || retval == 1) {
            // retval 0: GCT access
            // retval 1: RCT access
          }
          else if (retval == 2) {
            // refresh required
            curr_bank.rh_ref = true;
            num_refreshes_for_hydra++;
          }
          else if (retval == 3) {
            // RCT access and refresh
            curr_bank.rh_ref = true;
            curr_bank.rh_update = true;
            num_refreshes_for_hydra++;
            num_rct_updates++;
          }
        }
        // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
        if (rh_mode == rh_srs) {
          // update RRS's HRT
          bool do_swap = rrs[rank_num][bank_num].activate(page_num);
          if (do_swap) { // swap
            rrs_num_row_swap++;
            curr_bank.rh_swap = true;
          }
        }
        // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
        if (rh_mode == rh_abacus) {
          int retval = abacus[rank_num]->activate(bank_num, page_num);
          if (retval == 0) {
            // do nothing
          }
          else if (retval == 1) {
            curr_bank.rh_swap = true;
          }
          else if (retval == 2) {
            // refresh cycle
            curr_bank.rh_update = true;
          }
        }
        num_ab_activate += (access_agile) ? 1 : 0;
        // "RAMPART: RowHammer Mitigation and Repair for Sever Memory Systems," MEMSYS, 2023
        if (rh_mode == rh_rampart) {
          update_RAA_counter(rank_num, bank_num);
        }
        // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
        if (rh_mode == rh_prac) {
          uint64_t pnum = page_num % num_pages_per_bank;
          prac[rank_num][bank_num][pnum]++;
          if (prac[rank_num][bank_num][pnum] >= RAAIMT) {
            todo_RFM_flag[rank_num][bank_num] = true;
            prac[rank_num][bank_num][pnum] = 0;
          }
        }
        break;

      case mc_bank_activate:
      case mc_bank_read:
      case mc_bank_write:
        if (curr_bank.page_num != page_num)
        { // row miss
          curr_bank.action_time = curr_time;
          curr_bank.action_type_prev = curr_bank.action_type;
          curr_bank.th_id       = th_id;
          if ((cont_restore_after_write == false &&
               curr_bank.latest_write_time > curr_bank.latest_activate_time) ||
              (cont_restore_after_activate == false &&
               curr_bank.latest_write_time < curr_bank.latest_activate_time))
          {
            num_restore++;
          }

          // cont_precharge_after_write
          if ((curr_bank.latest_activate_time > curr_bank.latest_write_time &&
               cont_precharge_after_activate == true) ||
              (curr_bank.latest_activate_time < curr_bank.latest_write_time &&
               cont_precharge_after_write == true))
          {
            // update prediction_history
            if (tournament_interval > 0 && curr_bank.skip_pred == false && action_type_prev != mc_bank_activate)
            {
              num_l_pred_miss_curr += (curr_bank.local_bimodal_entry < 2) ? 1 : 0;
              num_l_pred_hit_curr  += (curr_bank.local_bimodal_entry > 1) ? 1 : 0;
              num_g_pred_miss_curr += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] < 2) ? 1 : 0;
              num_g_pred_hit_curr  += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] > 1) ? 1 : 0;
              num_o_pred_miss_curr++;
              num_c_pred_hit_curr++;
              curr_bank.local_bimodal_entry = (curr_bank.local_bimodal_entry == 0) ? 1 : 3;
              global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] = 
                (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] == 0) ? 1 : 3;
            }
            num_activate++;
            act_from_a_th[th_id]++;

            // "RAMPART: RowHammer Mitigation and Repair for Sever Memory Systems," MEMSYS, 2023
            if (rh_mode == rh_rampart) {
              update_RAA_counter(rank_num, bank_num);
            }
            // PRAC-4 "Chronus: Understanding and Securing the Cutting-Edge Industry Solutions to DRAM Read Disturbance," HPCA, 2025
            if (rh_mode == rh_prac) {
              uint64_t pnum = page_num % num_pages_per_bank;
              prac[rank_num][bank_num][pnum]++;
              if (prac[rank_num][bank_num][pnum] >= RAAIMT) {
                todo_RFM_flag[rank_num][bank_num] = true;
                prac[rank_num][bank_num][pnum] = 0;
              }
            }

            if (rh_mode == rh_blockhammer)
            { // BlockHammer
              my_bh->ACT(page_num, rank_num, bank_num, th_id, curr_time);
            }

            curr_bank.action_type = mc_bank_activate;
            curr_bank.page_num    = page_num;
            curr_bank.th_id       = th_id;
            last_activate_time[rank_num] = curr_time;
            curr_bank.latest_activate_time = curr_time;
            num_ab_activate += (access_agile) ? 1 : 0;

            // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
            if (rh_mode == rh_graphene) {
              uint32_t result_graphene = graphene[rank_num][bank_num].activate(page_num);
              if (result_graphene) {
                num_rh = num_rh + (2 * blast_radius);
                curr_bank.rh_ref = true;
                graphene_statistics[rank_num][bank_num] += (blast_radius * 2);
              }
            }
            // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
            if (rh_mode == rh_hydra) {
              int retval = hydra->activate(curr_time, address, 0);
              if (retval == 0 || retval == 1) {
                // retval 0: Only GCT hit --> do nothing
                // retval 1: RCT access
              }
              else if (retval == 2) {
                // refresh required
                curr_bank.rh_ref = true;
                num_refreshes_for_hydra++;
              }
              else if (retval == 3)
              {
                // RCT access and refresh
                curr_bank.rh_ref = true;
                curr_bank.rh_update = true;
                num_refreshes_for_hydra++;
                num_rct_updates++;
              }
            }
            // "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
            if (rh_mode == rh_srs) {
              bool result_rrs = rrs[rank_num][bank_num].activate(page_num);
              rrs_num_row_swap++;
              curr_bank.rh_swap = true;
            }
            // "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
            if (rh_mode == rh_abacus) {
              int retval = abacus[rank_num]->activate(bank_num, page_num);
              if (retval == 0) {
                // do nothing
              }
              else if (retval == 1) {
                curr_bank.rh_swap = true;
              }
              else if (retval == 2) {
                // refresh cycle
                curr_bank.rh_update = true;
              }
            }
          }
          else
          { // precharge
            num_precharge++;
            curr_bank.action_type = mc_bank_precharge;
            if (policy == mc_sched_a_open && a_open_fixed_to == false)
            {
              a_open_opc++;
            }

            // When we have to refresh the current bank, 
            if (curr_bank.rh_ref) {
              // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
              if (rh_mode == rh_para) {
                // TRR: delay current bank's action time (tRAS + tRP == tRC) 
                // preventive refresh to one adjacent row
                curr_bank.action_time += (tRAS_curr + tRP_curr) * process_interval;
              }
              // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
              if (rh_mode == rh_graphene) {
                // TRR: delay current bank's action time (2x TRR)
                curr_bank.action_time += 2 * (tRAS_curr + tRP_curr) * process_interval * blast_radius;
              }
              // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
              if (rh_mode == rh_hydra) {
                // TRR: delay current bank's action time
                curr_bank.action_time += (tRAS_curr + tRP_curr) * process_interval * 2 * blast_radius;
                hydra_ref++;
                if (curr_bank.rh_update) {
                  hydra_update++;
                  curr_bank.action_time += (2 * ((tRAS_curr + tRP_curr + tCL_curr + tBL * 2) * process_interval));
                  curr_bank.rh_update = false;
                }
              }
              curr_bank.rh_ref = false;
            }
          }
          curr_bank.skip_pred = false;
          break;
        }
        else
        { // row hit
          last_time_from_ab[rank_num][bank_num] = access_agile;
          switch (type)
          {
            case et_rd_dir_info_req:
            case et_rd_dir_info_rep:
            case et_read:
            case et_e_rd:
            case et_s_rd:
              // read
              if (curr_bank.action_type == mc_bank_activate && cont_restore_after_activate == true)
              {
                num_restore++;
              }
              if (curr_bank.action_type == mc_bank_activate && cont_precharge_after_activate == true)
              {
                num_precharge++;
              }
              if (is_last_time_write[rank_num] == true)
              {
                is_last_time_write[rank_num] = false;
                num_write_to_read_switch++;
              }
              num_read++;
              num_ab_read += (access_agile) ? 1 : 0;
              curr_bank.action_time = curr_time;
              curr_bank.th_id       = th_id;

              if (access_agile == true)
              {
                tCL_curr = tCL;
                mc_to_dir_t_curr = mc_to_dir_t;
                rd_miter = rd_dp_status.lower_bound(curr_time + tCL_ab*process_interval);
                if (rd_miter == rd_dp_status.end() || rd_miter->first >= curr_time + (tCL_ab+tBL_curr)*process_interval)
                {
                  tCL_curr = tCL_ab;
                  mc_to_dir_t_curr = mc_to_dir_t_ab;
                }
              }

              if (num_ecc_bursts > 0 && (get_col_num(address)%num_ecc_bursts) == 0)
              {
                ecc_bub = true;
              }

              for (uint32_t j = 0; j < tBL_curr; j++)
              {
                uint64_t next_time = curr_time + (tCL_curr+j)*process_interval;
                if ((j + 1) == tBL_curr && ecc_bub) 
                {
                  next_time += process_interval;
                }
                rd_dp_status.insert(pair<uint64_t, mc_bank_action>(next_time, mc_bank_read));
              }

              if (use_bank_group == true) {
                for (uint32_t j = 0; j < tBL_curr; j++) 
                {
                  if ((j + 1) == tBL_curr && ecc_bub)
                  {
                    j++;
                  }
                  bank_group_status.insert(pair<uint64_t, uint32_t>(curr_time + j * process_interval, rank_num * num_bank_groups + bank_group_num));
                }
              }
              last_read_time.first  = last_rank_num = rank_num;
              last_read_time.second = curr_time;
              if (num_ecc_bursts > 0 && ecc_bub) 
              {
                last_read_time_rank[rank_num] = last_rank_acc_time = curr_time + (tBL_curr + 1)*process_interval;
              }
              else
              {
                last_read_time_rank[rank_num] = last_rank_acc_time = curr_time + (tBL_curr)*process_interval;
              }

              // 3.PARBS
              if (par_bs == true) num_req_from_a_th[(*iter)->th_id]--;
              if ((*iter)->from.top()->type == ct_memory_controller)
              {
                delete *iter;
              }
              else
              {
                if (is_shared_llc) crossbar->add_mcrp_event(curr_time + mc_to_dir_t_curr, *iter, this);
                else directory->add_rep_event(curr_time + mc_to_dir_t_curr, *iter);
              }
              packet_time_in_mc_acc += (curr_time + mc_to_dir_t_curr - event_n_time[*iter]);
              event_n_time.erase(*iter);

              curr_bank.skip_pred = false;
              iter2 = iter;
              i2    = i;
              ++iter2;
              ++i2;
              for ( ; tournament_interval > 0 && iter2 != req_l.end() && i2 < req_window_sz; ++iter2, ++i2)
              {
                uint64_t address  = (*iter2)->address;
                if (rank_num == get_rank_num(address) &&
                    bank_num == get_bank_num(address, (*iter2)->th_id) &&
                    page_num == get_page_num(address))
                {
                  curr_bank.skip_pred   = true;
                  break;
                }
              }

              if (display_page_acc_pattern == true)
              {
                show_page_acc_pattern(th_id, rank_num, bank_num, page_num, mc_bank_read, curr_time);
              }

              if (curr_bank.skip_pred == true ||
                  policy == mc_sched_open || policy == mc_sched_m_open || policy == mc_sched_a_open ||
                  (policy == mc_sched_tournament && curr_tournament_idx == mc_pred_open) ||
                  ((policy == mc_sched_l_pred || (policy == mc_sched_tournament && curr_tournament_idx == mc_pred_local)) &&
                   curr_bank.local_bimodal_entry < 2) ||
                  ((policy == mc_sched_g_pred || (policy == mc_sched_tournament && curr_tournament_idx == mc_pred_global)) &&
                   global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] < 2))
              {
                curr_bank.action_type = mc_bank_read;
              }
              else
              {
                curr_bank.action_time += tRTP*process_interval;
                if (curr_bank.latest_activate_time > curr_bank.latest_write_time)
                { // only reads after activate
                  if (cont_restore_after_activate == false)
                  {
                    num_restore++;
                    if (tRTP < tRES_curr)
                    {
                      curr_bank.action_time -= tRTP*process_interval;
                    }
                    else
                    {
                      curr_bank.action_time -= tRES_curr*process_interval;
                    }
                  }
                  else if (cont_precharge_after_activate == true)
                  { // already precharged
                    curr_bank.action_time -= tRP*process_interval;
                    num_precharge--;

                  }
                }
                else
                { // there is at least one write after activate
                  if (cont_restore_after_write == false)
                  {
                    num_restore++;
                    if (tRTP < tRES_curr)
                    {
                      curr_bank.action_time -= tRTP*process_interval;
                    }
                    else
                    {
                      curr_bank.action_time -= tRES_curr*process_interval;
                    }
                  }
                  else if (cont_precharge_after_write == true)
                  { // already precharged
                    curr_bank.action_time -= tRP*process_interval;
                    num_precharge--;
                  }
                }
                curr_bank.action_type_prev = mc_bank_read;
                curr_bank.action_type = mc_bank_precharge;
                num_precharge++;

                // When we have to refresh the current bank,
                if (curr_bank.rh_ref) {
                  // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
                  if (rh_mode == rh_para) {
                    // TRR: delay current bank's action time (tRAS + tRP == tRC) 
                    // preventive refresh to one adjacent row
                    curr_bank.action_time += (tRAS_curr + tRP_curr) * process_interval;
                  }
                  // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
                  if (rh_mode == rh_graphene) {
                    // TRR: delay current bank's action time (2x TRR)
                    curr_bank.action_time += 2 * (tRAS_curr + tRP_curr) * process_interval * blast_radius;
                  }
                  // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
                  if (rh_mode == rh_hydra) {
                    // TRR: delay current bank's action time
                    curr_bank.action_time += (tRAS_curr + tRP_curr) * process_interval * 2 * blast_radius;
                    hydra_ref++;
                    if (curr_bank.rh_update) {
                      hydra_update++;
                      curr_bank.action_time += (2 * ((tRAS_curr + tRP_curr + tCL_curr + tBL * 2) * process_interval));
                      curr_bank.rh_update = false;
                    }
                  }
                  curr_bank.rh_ref = false;
                }
              }

              // 1. BlockHammer
              if (rh_mode == rh_blockhammer && my_bh->attackthrottler == true) {
                if (my_bh->attackthrottlers[rank_num][bank_num].first->get_valid()) { // TODO - weird structure WRONG!! update both!! (use only one!!)
                  my_bh->attackthrottlers[rank_num][bank_num].first->update_used_quota(th_id, false); // decrement
                  my_bh->attackthrottlers[rank_num][bank_num].second->update_used_quota(th_id, false); // decrement
                }
                else {
                  my_bh->attackthrottlers[rank_num][bank_num].first->update_used_quota(th_id, false); // decrement
                  my_bh->attackthrottlers[rank_num][bank_num].second->update_used_quota(th_id, false); // decrement
                }
              }
              // bliss, par-bs
              if (par_bs == true)
              {
                if (curr_batch_last == (int32_t)i)
                {
                  if (i == 0)
                  {
                    curr_batch_last = (int32_t)req_l.size() - 2;
                    if (curr_batch_last > (int32_t)req_window_sz - 1) curr_batch_last = req_window_sz - 1;
                  }
                  else
                  {
                    curr_batch_last--;
                  }
                }
                else if (curr_batch_last > (int32_t)i)
                {
                  curr_batch_last--;
                }
              }
              if (bliss) {
                my_bliss->update_blacklist(th_id);
              }
              
              /* RowHammer attacks (for Denial of Service) */
              if (use_attacker)
              {
                for (size_t t = 0; t < num_rowhammer_attackers; ++t) 
                {
                  if (th_id == hammer_threads[t]->attacker_thread_number) 
                  {
                    hammer_threads[t]->attacker_req_count -= 1;
                  }
                }
              }
              /* end of RowHammer attacks (for Denial of Service) */

              req_l.erase(iter);

              if (policy == mc_sched_a_open && a_open_fixed_to == false)
              {
                a_open_win_cnt--;
                if (a_open_win_cnt == 0)
                {
                  a_open_win_cnt = a_open_win_sz;  // build new window
                  // tA_OPEN_TO update
                  if (a_open_ppc > a_open_ppc_th)
                  {
                    tA_OPEN_TO += tA_OPEN_TO_DELTA;
                  }
                  else if (a_open_opc > a_open_opc_th && tA_OPEN_TO >= tA_OPEN_TO_DELTA)
                  {
                    tA_OPEN_TO -= tA_OPEN_TO_DELTA;
                  }
                  else
                  {
                  }
                  a_open_opc= 0;   // opc reset
                  a_open_ppc= 0;   // ppc reset
                }
              }
              break;
            case et_evict:
            case et_evict_owned:
            case et_dir_evict:
            case et_s_rd_wr:
              // write
              if (cont_restore_after_write == true)
              {
                num_restore++;
              }
              if (cont_precharge_after_write == true)
              {
                num_precharge++;
              }
              is_last_time_write[rank_num] = true;
              num_write++;
              num_ab_write += (access_agile) ? 1 : 0;
              curr_bank.latest_write_time = curr_time;
              curr_bank.action_time = curr_time;
              curr_bank.th_id       = th_id;

              if (last_time_from_ab[rank_num][bank_num] == true)
              {
                tCL_curr = tCL;
                mc_to_dir_t_curr = mc_to_dir_t;
                wr_miter = wr_dp_status.lower_bound(curr_time + tCL_ab*process_interval);
                if (wr_miter == wr_dp_status.end() || wr_miter->first >= curr_time + (tCL_ab+tBL_curr)*process_interval)
                {
                  tCL_curr = tCL_ab;
                  mc_to_dir_t_curr = mc_to_dir_t_ab;
                }
              }

              if (num_ecc_bursts > 0 && (get_col_num(address)%num_ecc_bursts) == 0)
              {
                ecc_bub = true;
              }

              for (uint32_t j = 0; j < tBL_curr; j++)
              {
                uint64_t next_time = curr_time + (tCL_curr+j)*process_interval;
                if ((j + 1) == tBL_curr && ecc_bub) 
                {
                  next_time += process_interval;
                }
                wr_dp_status.insert(pair<uint64_t, mc_bank_action>(next_time, mc_bank_write));
              }
              if (use_bank_group == true) {
                for (uint32_t j = 0; j < tBL_curr; j++) 
                {
                  if ((j + 1) == tBL_curr && ecc_bub)
                  {
                    j++;
                  }
                  bank_group_status.insert(pair<uint64_t, uint32_t>(curr_time + j * process_interval, rank_num * num_bank_groups + bank_group_num));
                }
              }
              last_rank_num = rank_num;
              if (num_ecc_bursts > 0 && ecc_bub) 
              {
                last_write_time[rank_num] = curr_time + (tCL_curr+tBL_curr+1)*process_interval;
              }
              else
              {
                last_write_time[rank_num] = curr_time + (tCL_curr+tBL_curr)*process_interval;
              }

              // 3.PARBS
              if (par_bs == true) num_req_from_a_th[(*iter)->th_id]--;
              if (type == et_s_rd_wr)
              {
                (*iter)->type = et_s_rd;
                if ((*iter)->from.top()->type == ct_memory_controller)
                {
                  delete *iter;
                }
                else
                {
                  if (is_shared_llc) crossbar->add_mcrp_event(curr_time + mc_to_dir_t_curr, *iter, this);
                  else directory->add_rep_event(curr_time + mc_to_dir_t_curr, *iter);
                }
                packet_time_in_mc_acc += (curr_time + mc_to_dir_t_curr - event_n_time[*iter]);
                event_n_time.erase(*iter);
              }
              else
              {
                event_n_time.erase(*iter);
                delete *iter;
              }

              curr_bank.skip_pred = false;
              iter2 = iter;
              i2    = i;
              ++iter2;
              ++i2;
              for ( ; tournament_interval > 0 && iter2 != req_l.end() && i2 < req_window_sz; ++iter2, ++i2)
              {
                uint64_t address  = (*iter2)->address;
                if (rank_num == get_rank_num(address) &&
                    bank_num == get_bank_num(address, (*iter2)->th_id) &&
                    page_num == get_page_num(address))
                {
                  curr_bank.skip_pred   = true;
                  break;
                }
              }

              if (display_page_acc_pattern == true)
              {
                show_page_acc_pattern(th_id, rank_num, bank_num, page_num, mc_bank_write, curr_time);
              }

              if (curr_bank.skip_pred == true ||
                  policy == mc_sched_open || policy == mc_sched_m_open || policy == mc_sched_a_open ||
                  (policy == mc_sched_tournament && curr_tournament_idx == mc_pred_open) ||
                  ((policy == mc_sched_l_pred || (policy == mc_sched_tournament && curr_tournament_idx == mc_pred_local)) &&
                   curr_bank.local_bimodal_entry < 2) ||
                  ((policy == mc_sched_g_pred || (policy == mc_sched_tournament && curr_tournament_idx == mc_pred_global)) &&
                   global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] < 2))
              {
                curr_bank.action_type = mc_bank_write;
              }
              else
              {
                curr_bank.action_time += tWTP*process_interval;
                // there is at least one write after activate
                if (cont_restore_after_write == false)
                {
                  num_restore++;
                  if (tWTP < tRES_curr)
                  {
                    curr_bank.action_time -= tWTP*process_interval;
                  }
                  else
                  {
                    curr_bank.action_time -= tRES_curr*process_interval;
                  }
                }
                else if (cont_precharge_after_write == true)
                { // already precharged
                  curr_bank.action_time -= tRP*process_interval;
                  num_precharge--;

                }
                curr_bank.action_type_prev = mc_bank_write;
                curr_bank.action_type = mc_bank_precharge;
                num_precharge++;
                
                // When we have to refresh the current bank, 
                if (curr_bank.rh_ref) {
                  // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
                  if (rh_mode == rh_para) {
                    // TRR: delay current bank's action time (tRAS + tRP == tRC) 
                    // preventive refresh to one adjacent row
                    curr_bank.action_time += (tRAS_curr + tRP_curr) * process_interval;
                  }
                  // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
                  if (rh_mode == rh_graphene) {
                    // TRR: delay current bank's action time (2x TRR)
                    curr_bank.action_time += 2 * (tRAS_curr + tRP_curr) * process_interval * blast_radius;
                  }
                  // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
                  if (rh_mode == rh_hydra) {
                    // TRR: delay current bank's action time
                    curr_bank.action_time += (tRAS_curr + tRP_curr) * process_interval * 2 * blast_radius;
                    hydra_ref++;
                    if (curr_bank.rh_update) {
                      hydra_update++;
                      curr_bank.action_time += (2 * ((tRAS_curr + tRP_curr + tCL_curr + tBL * 2) * process_interval));
                      curr_bank.rh_update = false;
                    }
                  }
                  curr_bank.rh_ref = false;
                }
              }

              // BlockHammer
              if (rh_mode == rh_blockhammer && my_bh->attackthrottler == true) {
                if (my_bh->attackthrottlers[rank_num][bank_num].first->get_valid()) { // TODO - weird structure
                  my_bh->attackthrottlers[rank_num][bank_num].first->update_used_quota(th_id, false); // decrement
                  my_bh->attackthrottlers[rank_num][bank_num].second->update_used_quota(th_id, false); // decrement
                }
                else {
                  my_bh->attackthrottlers[rank_num][bank_num].first->update_used_quota(th_id, false); // decrement
                  my_bh->attackthrottlers[rank_num][bank_num].second->update_used_quota(th_id, false); // decrement
                }
              }
              // BLISS, PAR-BS
              if (par_bs == true) {
                if (curr_batch_last == (int32_t)i) {
                  if (i == 0) {
                    curr_batch_last = (int32_t)req_l.size() - 2;
                    if (curr_batch_last > (int32_t)req_window_sz - 1) {
                      curr_batch_last = req_window_sz - 1;
                    }
                  }
                  else {
                    curr_batch_last--;
                  }
                }
                else if (curr_batch_last > (int32_t)i) {
                  curr_batch_last--;
                }
              }
              if (bliss) {
                my_bliss->update_blacklist(th_id);
              }

              /* RowHammer attacks (for Denial of Service) */
              if (use_attacker)
              {
                for (size_t t = 0; t < num_rowhammer_attackers; ++t) 
                {
                  if (th_id == hammer_threads[t]->attacker_thread_number) 
                  {
                    hammer_threads[t]->attacker_req_count -= 1;
                  }
                }
              }
              /* end of RowHammer attacks (for Denial of Service) */

              req_l.erase(iter);
              if (policy == mc_sched_a_open && a_open_fixed_to == false)
              {
                a_open_win_cnt--;
                if (a_open_win_cnt == 0)
                {
                  a_open_win_cnt = a_open_win_sz;  // build new window
                  // tA_OPEN_TO update
                  if (a_open_ppc > a_open_ppc_th)
                  {
                    tA_OPEN_TO += tA_OPEN_TO_DELTA;
                  }
                  else if (a_open_opc > a_open_opc_th && tA_OPEN_TO >= tA_OPEN_TO_DELTA)
                  {
                    tA_OPEN_TO -= tA_OPEN_TO_DELTA;
                  }
                  else
                  {
                  }
                  a_open_opc= 0;   // opc reset
                  a_open_ppc= 0;   // ppc reset
                }
              }
              break;
            default:
              cout << "action_type = " << curr_bank.action_type << endl;
              show_state(curr_time);  ASSERTX(0);
              break;
          }
          // update prediction_history
          if (tournament_interval > 0 && curr_bank.skip_pred == false && action_type_prev != mc_bank_activate)
          {
            num_l_pred_hit_curr  += (curr_bank.local_bimodal_entry < 2) ? 1 : 0;
            num_l_pred_miss_curr += (curr_bank.local_bimodal_entry > 1) ? 1 : 0;
            num_g_pred_hit_curr  += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] < 2) ? 1 : 0;
            num_g_pred_miss_curr += (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] > 1) ? 1 : 0;
            num_o_pred_hit_curr++;
            num_c_pred_miss_curr++;
            curr_bank.local_bimodal_entry = (curr_bank.local_bimodal_entry == 3) ? 2 : 0;
            global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] = 
              (global_bimodal_entry[th_id][pred_history[th_id]%num_history_patterns] == 3) ? 2 : 0;
          }
          if (tournament_interval > 0 && curr_bank.skip_pred == false)
          {
            pred_history[th_id] = (pred_history[th_id] << 1) + (curr_bank.action_type == mc_bank_precharge ? 1 : 0);
          }
          curr_bank.skip_pred = false;
        }
        break;
      default:
        cout << "curr (rank, bank) = (" << rank_num << ", " << bank_num << ")" << endl;
        show_state(curr_time);  ASSERTX(0);
        break;
    }
  }
  else if (policy == mc_sched_m_open || policy == mc_sched_a_open)
  {
    for (uint32_t i = 0; i < num_ranks_per_mc; i++)
    {
      for(uint32_t k = 0; k < num_banks_per_rank + num_banks_per_rank_ab; k++)
      {
        BankStatus & curr_bank = bank_status[i][k];

        if (curr_bank.action_type == mc_bank_read || curr_bank.action_type == mc_bank_write)
        { 
	        if ((curr_bank.action_type == mc_bank_read) && curr_bank.action_time + tBBL*process_interval > curr_time)
	        {
	        }
	        else if ((curr_bank.action_type == mc_bank_write) && curr_bank.action_time + tBBLW*process_interval > curr_time)
	        {
	        }
          else
          {
            if ((curr_bank.action_type == mc_bank_read  && (curr_bank.action_time+tRTP*process_interval > curr_time)) ||
                (curr_bank.action_type == mc_bank_write && (curr_bank.action_time+tWTP*process_interval > curr_time)))
            { // tRTP or tWTP
              continue;
            }
            if (curr_time < curr_bank.latest_activate_time + (last_time_from_ab[i][k] ? tRAS_ab : tRAS)*process_interval)
            { // tRAS
              continue;
            }
            if ((policy == mc_sched_m_open) && (curr_time < curr_bank.latest_activate_time + tM_OPEN_TO*process_interval))
            { // tM_OPEN_TO
              continue;
            }
            if ((policy == mc_sched_a_open) && (curr_time < curr_bank.action_time + tA_OPEN_TO*process_interval))
            { // tA_OPEN_TO
              continue;
            }
            curr_bank.action_time = curr_time;
            curr_bank.action_type_prev = curr_bank.action_type;
            num_precharge++;
            curr_bank.action_type = mc_bank_precharge;

            // When we have to refresh the current bank, 
            if (curr_bank.rh_ref) {
              // "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA, 2014
              if (rh_mode == rh_para) {
                // TRR: delay current bank's action time (tRAS + tRP == tRC) 
                // preventive refresh to one adjacent row
                curr_bank.action_time += (tRAS + tRP) * process_interval;
              }
              // "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
              if (rh_mode == rh_graphene) {
                // TRR: delay current bank's action time (2x TRR)
                curr_bank.action_time += 2 * (tRAS + tRP) * process_interval * blast_radius;
              }
              // "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
              if (rh_mode == rh_hydra) {
                // TRR: delay current bank's action time
                curr_bank.action_time += (tRAS + tRP) * process_interval * 2 * blast_radius;
                hydra_ref++;
                if (curr_bank.rh_update) {
                  hydra_update++;
                  curr_bank.action_time += (2 * ((tRAS + tRP + tCL + tBL * 2) * process_interval));
                  curr_bank.rh_update = false;
                }
              }
              curr_bank.rh_ref = false;
            }

            geq->add_event(curr_time + process_interval, this);
            break;
          }
        }
      }
    }
  }

  if (req_l.empty() == false)
  {
    geq->add_event(curr_time + process_interval, this);
  }

  return 0;
}

// [RFM]
void MemoryController::update_RAA_counter(uint32_t rank_num, uint32_t bank_num) {
  RAA_counter[rank_num][bank_num]++;

  if (RAA_counter[rank_num][bank_num] >= RAAIMT) {
    todo_RFM_flag[rank_num][bank_num] = true;
  }
}

void MemoryController::show_RAA_counter() {
  cout << "[Show RAA_counters] MC[" << num << "]" << endl;
  for (int i = 0; i < num_ranks_per_mc; i++) {
    for (int j = 0; j < num_banks_per_rank; j++) {
      cout << "RAA_counter[" << i << "][" << j << "] " << RAA_counter[i][j] << endl;
    }
  }
}

bool MemoryController::pre_processing(uint64_t curr_time)
{
  // update and choose curr_tournament_idx
  if (tournament_interval > 0 && (num_read + num_write - num_acc_till_last_interval >= tournament_interval))
  {
    num_acc_till_last_interval = num_read + num_write;
    num_t_pred_miss += (curr_tournament_idx == mc_pred_open)   ? num_o_pred_miss_curr :
      (curr_tournament_idx == mc_pred_closed) ? num_c_pred_miss_curr :
      (curr_tournament_idx == mc_pred_local)  ? num_l_pred_miss_curr : num_g_pred_miss_curr;
    num_t_pred_hit  += (curr_tournament_idx == mc_pred_open)   ? num_o_pred_hit_curr  :
      (curr_tournament_idx == mc_pred_closed) ? num_c_pred_hit_curr  :
      (curr_tournament_idx == mc_pred_local)  ? num_l_pred_hit_curr  : num_g_pred_hit_curr;

    mc_pred_tournament_idx new_idx = mc_pred_open;
    uint64_t num_hit = num_o_pred_hit_curr;
    if (num_c_pred_hit_curr > num_hit) { new_idx = mc_pred_closed; num_hit = num_c_pred_hit_curr; }
    if (num_l_pred_hit_curr > num_hit) { new_idx = mc_pred_local;  num_hit = num_l_pred_hit_curr; }
    if (num_g_pred_hit_curr > num_hit) { new_idx = mc_pred_global; num_hit = num_g_pred_hit_curr; }

    // bimodal here as well
    if (curr_best_predictor_idx == new_idx)
    {
      curr_tournament_idx = new_idx;
    }
    curr_best_predictor_idx = new_idx;
    num_l_pred_miss += num_l_pred_miss_curr;  num_l_pred_miss_curr = 0;
    num_g_pred_miss += num_g_pred_miss_curr;  num_g_pred_miss_curr = 0;
    num_o_pred_miss += num_o_pred_miss_curr;  num_o_pred_miss_curr = 0;
    num_c_pred_miss += num_c_pred_miss_curr;  num_c_pred_miss_curr = 0;
    num_l_pred_hit  += num_l_pred_hit_curr;   num_l_pred_hit_curr  = 0;
    num_g_pred_hit  += num_g_pred_hit_curr;   num_g_pred_hit_curr  = 0;
    num_o_pred_hit  += num_o_pred_hit_curr;   num_o_pred_hit_curr  = 0;
    num_c_pred_hit  += num_c_pred_hit_curr;   num_c_pred_hit_curr  = 0;
  }

  multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    // BlockHammer
    if (rh_mode == rh_blockhammer && my_bh->attackthrottler == true) {
      uint64_t th_id = req_event_iter->second->th_id;
      uint64_t addr = req_event_iter->second->address;
      uint32_t rank_num = get_rank_num(addr);
      uint32_t bank_num = get_bank_num(addr, th_id);
      auto cur = my_bh->attackthrottlers[rank_num][bank_num];

      bool valid_BL_ACTs = (cur.first->get_valid() == true) ? true : false;
      bool quota_full;

      if (valid_BL_ACTs) {
        if (quota_full = cur.first->query(th_id,rank_num,bank_num,num))
          continue; // if quota is full, no more in-flight requests
        else {
          cur.first->update_used_quota(th_id, true); // increment
          cur.second->update_used_quota(th_id, true); // increment
        }
      }
      else {
        if (quota_full = cur.second->query(th_id,rank_num,bank_num,num))
          continue; // if quota is full, no more in-flight requests
        else {
          cur.first->update_used_quota(th_id, true); // increment
          cur.second->update_used_quota(th_id, true); // increment
        }
      }
    }
    // PAR-BS, BLISS
    if (par_bs == true) {
      num_req_from_a_th[req_event_iter->second->th_id]++;
    }
    req_l.push_back(req_event_iter->second);
    acc_from_a_th[req_event_iter->second->th_id]++;
    ++req_event_iter;
  }


  /* RowHammer attacks (for Denial of Service) */
  if (use_attacker)
  { // generate row hammer
    for (size_t t = 0; t < num_rowhammer_attackers; ++t) 
    {
      uint32_t curr = hammer_threads[t]->attacker_thread_number;
      if (hammer_threads[t]->attacker_req_count <= hammer_threads[t]->attacker_max_req_count)
      {
        req_l.push_back(hammer_threads[t]->make_event(this));
        hammer_threads[t]->attacker_req_count += 1;
        if (par_bs)
        {
          num_req_from_a_th[curr]++;
        }
      }
    }
  }
  /* end of RowHammer attacks (for Denial of Service) */

  // 1.BlockHammer
  // 3.PARBS
  if (par_bs == true && curr_batch_last == -1 && req_l.size() > 0)
   {
    curr_batch_last = (int32_t)req_l.size() - 1;
    if (curr_batch_last > (int32_t)req_window_sz - 1) curr_batch_last = req_window_sz - 1;
  }
  req_event.erase(curr_time);

  bool command_sent = false;
  vector<LocalQueueElement *>::iterator iter, iter2;

  return command_sent;
}


void MemoryController::update_acc_dist()
{
  map<uint64_t, uint64_t>::iterator p_iter, c_iter;

  for (c_iter = os_page_acc_dist_curr.begin(); c_iter != os_page_acc_dist_curr.end(); ++c_iter)
  {
    p_iter = os_page_acc_dist.find(c_iter->first);

    if (p_iter == os_page_acc_dist.end())
    {
      os_page_acc_dist.insert(pair<uint64_t, uint64_t>(c_iter->first, 1));
    }
    else
    {
      p_iter->second += c_iter->second;
    }
  }
  os_page_acc_dist_curr.clear();
}

// "Graphene: Strong yet Lightweight Row Hammer Protection," MICRO, 2020
MemoryController::Graphene_entry::Graphene_entry()
{
  address = (uint64_t)1 << 62;
  count = (uint64_t)0;
}

MemoryController::Graphene_entry::~Graphene_entry()
{
}

uint64_t MemoryController::Graphene::get_address(int idx)
{
  return entries[idx].get_address();
}

uint64_t MemoryController::Graphene::get_count(int idx)
{
  return entries[idx].get_count();
}

uint64_t MemoryController::Graphene::activate(uint64_t addr)
{
  int i;
  uint64_t dest;
  for (i = 0; i < size; ++i)
  {
    if (entries[i].get_address() == addr)
    { // found, then increment count
      dest = i;
      entries[dest].increment_count();
      break;
    }
  }
  if (i == size)
  { // not found, replace one
    dest = size - 1;
    entries[dest].set_address(addr);
    entries[dest].increment_count();
  }
  for (i = dest; i > 0; --i)
  { // sort
    if (entries[i].get_count() <= entries[i-1].get_count())
    {
      break;
    }
    else
    {
      Graphene_entry temp;
      temp = entries[i];
      entries[i] = entries[i-1];
      entries[i-1] = temp;
      --dest;
    }
  }
  if (entries[dest].get_count() % threshold == 0)
  { // target row refresh
    return 1;
  }
  return 0;
}

void MemoryController::Graphene::debug()
{
  uint64_t address, count;
  int i;
  for (i = 0; i < size; ++i)
  {
    address = entries[i].get_address();
    count = entries[i].get_count();
    cout << "Entry [" << i << "]: " << hex << address << ", " << dec << count << endl;
  }
}

void MemoryController::Graphene::flush()
{
  int i;
  for (i = 0; i < size; ++i)
  {
    entries[i].reset_address();
    entries[i].reset_count();
  }
}

// "BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows," HPCA, 2021
MemoryController::BlockHammer::BlockHammer(BlockHammerParameters &p)
{
  // CBF
  for (uint32_t i = 0; i < p.num_ranks_per_mc; i++) {
    std::vector<pair<RowBlockerCBF *, RowBlockerCBF *> > temp_vector;
    for (uint32_t j = 0; j < p.num_banks_per_rank; j++) {
      std::pair<RowBlockerCBF *, RowBlockerCBF *> temp;
      temp.first = new RowBlockerCBF(p);
      temp.first->validate(true);
      temp.second = new RowBlockerCBF(p);
      temp.second->validate(false);
      temp_vector.push_back(temp);
    }
    rowblocker_CBFs.push_back(temp_vector);
  }

  // HB
  for (uint32_t i = 0; i < p.num_ranks_per_mc; i++) {
    RowBlockerHB * temp = new RowBlockerHB(p);
    rowblocker_HBs.push_back(temp);
  }

  // AttackThrottler
  attackthrottler = p.attackthrottler;
  for (uint32_t i = 0; i < p.num_ranks_per_mc; i++) {
    std::vector<std::pair<AttackThrottler*,AttackThrottler*>> temp_vector;
    for (uint32_t i = 0; i < p.num_banks_per_rank; i++) {
      std::pair<AttackThrottler*,AttackThrottler*> temp;
      temp.first = new AttackThrottler(p);
      temp.first->validate(true);
      temp.second = new AttackThrottler(p);
      temp.second->validate(false);
      temp_vector.push_back(temp);
    }
    attackthrottlers.push_back(temp_vector);
  }

  rowblocker_throttled = 0; // global value, for debug
}

MemoryController::BlockHammer::~BlockHammer() {
  std::cout << "rowblocker_throttled: " << rowblocker_throttled << std::endl;

  //
  for (uint32_t i = 0; i < mc->num_ranks_per_mc; i++) {
    std::cout << std::endl << "HB[rank"<<i<<"]"<<std::endl;
    rowblocker_HBs[i]->print_HB();
  }
}

uint64_t MemoryController::BlockHammer::ACT(
    uint64_t _row_num, uint32_t _rank_num, uint32_t _bank_num, uint64_t _th_id, uint64_t _curr_time) {
  // exception check
  if (_row_num == -1) {
    cout << "row_num == -1, breakdown" << endl; assert(0); }
  auto curr_CBFs = rowblocker_CBFs[_rank_num][_bank_num];
  /* */
  uint32_t valid_CBF = -1;
  if (curr_CBFs.first->get_valid() == true)
    valid_CBF = 0;
  else if (curr_CBFs.second->get_valid() == true)
    valid_CBF = 1;

  // 1. update CBF
  uint64_t BL_address;
  if (valid_CBF == 0) {
    if (curr_CBFs.first->get_valid() == false) {
      std::cout << "accessed invalid CBF first" << std::endl; assert(0); }
    BL_address = rowblocker_CBFs[_rank_num][_bank_num].first->update(_row_num);
    rowblocker_CBFs[_rank_num][_bank_num].second->update(_row_num);
  }
  else if (valid_CBF == 1) {
    if (curr_CBFs.second->get_valid() == false) {
      std::cout << "accessed invalid CBF second" << std::endl; assert(0); }
    rowblocker_CBFs[_rank_num][_bank_num].first->update(_row_num);
    BL_address = rowblocker_CBFs[_rank_num][_bank_num].second->update(_row_num);
  }
  else {
    cout << "wrong valid_CBF number of non 0 or 1" << endl; assert(0); }

  // 2. update HB
  bool recent_ACT;
  recent_ACT = rowblocker_HBs[_rank_num]->update(_row_num, _bank_num, _rank_num, mc->num, _th_id, _curr_time);

  // 3. update AttackThrottler
  if (attackthrottler == true) {
  auto cur_attackthrottlers = attackthrottlers[_rank_num][_bank_num];
  uint32_t valid_attackthrottler = (cur_attackthrottlers.first->get_valid() == true) ? true : false;
    if (BL_address == _row_num) {
      if (valid_attackthrottler == true) {
        cur_attackthrottlers.first->update_BL_ACTs(_th_id);
      }
      else {
        cur_attackthrottlers.second->update_BL_ACTs(_th_id);
      }
    }
  }

  // check whether blacklist or not
  if (BL_address == _row_num && recent_ACT == true)
    return _row_num;
  else
    return -1;
}

uint64_t MemoryController::BlockHammer::query_rowblocker(
    uint64_t _row_num, uint32_t _rank_num, uint32_t _bank_num, uint64_t _th_id, uint64_t _curr_time) {

  // execption check
  auto curr_CBFs = rowblocker_CBFs[_rank_num][_bank_num];

  // find current CBF
  uint32_t valid_CBF = -1;
  if (curr_CBFs.first->get_valid() == true)
    valid_CBF = 0;
  else if (curr_CBFs.second->get_valid() == true)
    valid_CBF = 1;

  // query CBF
  uint64_t BL_address;
  if (valid_CBF == 0) {
    if (rowblocker_CBFs[_rank_num][_bank_num].first->get_valid() == false) {
      std::cout << "accessed invalid CBF first" << std::endl; assert(0); }
    BL_address = rowblocker_CBFs[_rank_num][_bank_num].first->query(_row_num);
  }
  else if (valid_CBF == 1) {
    if (rowblocker_CBFs[_rank_num][_bank_num].second->get_valid() == false) {
      std::cout << "accessed invalid CBF second" << std::endl; assert(0); }
    BL_address = rowblocker_CBFs[_rank_num][_bank_num].second->query(_row_num);
  }
  else {
    cout << "wrong valid_CBF number" << endl; assert(0); }

  // query HB
  bool recent_ACT;
  recent_ACT = rowblocker_HBs[_rank_num]->query(_row_num, _bank_num, _th_id, _curr_time);


  // check whether blacklist or not
  if (BL_address == _row_num && recent_ACT == true)
    return _row_num;
  else
    return -1;
}

void MemoryController::BlockHammer::print_blockhammer() {
  std::vector< vector < pair<RowBlockerCBF *,RowBlockerCBF *> > >::iterator vec_iter;
  std::vector< pair<RowBlockerCBF *,RowBlockerCBF *> >::iterator iter;
  std::vector<RowBlockerHB *>::iterator hb_iter = rowblocker_HBs.begin();
  uint32_t i,j = 0; // rank, bank
}

// [CBF]
MemoryController::RowBlockerCBF::RowBlockerCBF(BlockHammerParameters &p) {
  
  valid = false;
  hash = p.hash;

  blistTH = p.blistTH;
  cntSize = p.cntSize;
  totNumHashes = p.totNumHashes;

  num_reset = 0;

  for (uint32_t i = 0; i < cntSize; i++)
    cntTable.push_back(0);
}

MemoryController::RowBlockerCBF::~RowBlockerCBF() {
}

uint64_t MemoryController::RowBlockerCBF::update(uint64_t _row_num) {
  // update CBF table & find minimum
  uint64_t report_minimum = -1; // max valu for uint32_t
  for (uint32_t i = 0; i < totNumHashes; i++) {
    uint32_t idx = lookup_hash(_row_num, i) % cntSize;
    cntTable[idx]++;

    if (report_minimum > cntTable[idx]) {
      report_minimum = cntTable[idx];
    }
  }

  if (report_minimum >= blistTH)
    return _row_num;
  else
    return -1;
}

uint64_t MemoryController::RowBlockerCBF::query(uint64_t _row_num) {

  // find minimum
  uint64_t report_minimum = -1; // max valu for uint32_t
  for (uint32_t i = 0; i < totNumHashes; i++) {
    uint32_t idx = lookup_hash(_row_num, i) % cntSize;

    if (report_minimum > cntTable[idx]) {
      report_minimum = cntTable[idx];
    }
  }

  if (report_minimum >= blistTH)
    return _row_num;
  else
    return -1;
}

void MemoryController::RowBlockerCBF::reset() {
  for (auto iter = cntTable.begin(); iter != cntTable.end(); iter++)
    (*iter) = 0;
  num_reset++;
}

void MemoryController::RowBlockerCBF::validate(bool _valid) {
  valid = _valid;
}

bool MemoryController::RowBlockerCBF::get_valid() {
  return valid;
}

uint32_t MemoryController::RowBlockerCBF::lookup_hash(uint64_t _row_num, uint32_t hash_number) {
	return this->hash->lookup_hash(_row_num, hash_number);
}

void MemoryController::RowBlockerCBF::print_CBF() {
  std::vector<uint64_t>::iterator iter;
  
  // skip if empty
  bool is_empty = true;
  for (iter = cntTable.begin(); iter != cntTable.end(); iter++) {
    if ((*iter) != 0)
      is_empty = false;
  }
  if (is_empty)
    return;

  //
  uint32_t i = 0; 
  for (iter = cntTable.begin(); iter != cntTable.end(); i++,iter++) {
    //std::cout << "[" << i << "] " << (*iter) << std::endl;
    std::cout << i << "," << (*iter) << std::endl;
  }
  std::cout << "valid," << valid << std::endl;
  std::cout << "num_reset," << num_reset << std::endl;
}

uint64_t MemoryController::RowBlockerCBF::sum_CBF() {
  uint64_t sum = 0;
  for (auto iter = cntTable.begin(); iter != cntTable.end(); iter++) {
    sum += (*iter);
  }
  return sum;
}

MemoryController::RowBlockerHB::RowBlockerHB(BlockHammerParameters &p) {
  historySize = p.historySize;
  Delay = p.Delay;
  for (uint32_t i = 0; i < 32; i++) { // TODO
    std::multimap<uint64_t,historyEntry*> temp_vector;
    historyBuffer.push_back(temp_vector);
  }
}

bool MemoryController::RowBlockerHB::update(
    uint64_t _row_num, uint32_t _bank_num, uint32_t _rank_num, uint32_t num, uint64_t _th_id, uint64_t _curr_time) {
  // delete all expired
  uint32_t debug_i = 0;
  uint32_t lower_bound = (_curr_time > Delay) ? (_curr_time-Delay) : 0; // TODO tDelay = 10000
  auto expired_iter = historyBuffer[_bank_num].lower_bound(lower_bound); 
  for (auto iter = historyBuffer[_bank_num].begin(); iter != expired_iter; debug_i++,iter++) {
    if ((*iter).first >= lower_bound)
      break;
    delete (*iter).second;
  }
  historyBuffer[_bank_num].erase(historyBuffer[_bank_num].begin(), expired_iter);

  // query
  bool target_match = false;

  // insert
  historyEntry *temp = new historyEntry;
  temp->bankNum = _bank_num;
  temp->pageNum = _row_num;
  historyBuffer[_bank_num].insert(std::pair<uint64_t,historyEntry *>(_curr_time,temp));

  return target_match;
}

bool MemoryController::RowBlockerHB::query(
    uint64_t _row_num, uint32_t _bank_num, uint64_t _th_id, uint64_t _curr_time) {

  // delete all expired
  uint32_t debug_i = 0;
  uint32_t lower_bound = (_curr_time > Delay) ? (_curr_time-Delay) : 0; // TODO tDelay = 10000
  auto expired_iter = historyBuffer[_bank_num].lower_bound(lower_bound); 
  for (auto iter = historyBuffer[_bank_num].begin(); iter != expired_iter; debug_i++,iter++) {
    if ((*iter).first >= lower_bound)
      break;
    delete (*iter).second;
  }
  historyBuffer[_bank_num].erase(historyBuffer[_bank_num].begin(), expired_iter);

  // query
  bool target_match = false; 
  for (auto iter = historyBuffer[_bank_num].begin(); iter != historyBuffer[_bank_num].end(); iter++) {
    if ((*iter).second->bankNum == _bank_num && (*iter).second->pageNum == _row_num)
      target_match = true; 
  }

  // return
  return target_match;
}

void MemoryController::RowBlockerHB::print_HB() {
  uint32_t idx = 0;
  if (historyBuffer.size() == 0)
    std::cout << "0" << std::endl;
}

// [AttackThrottler]
MemoryController::AttackThrottler::AttackThrottler(BlockHammerParameters &p) {
  valid = false;
  quota_base = p.quota_base;
  blistTH = p.blistTH;
  Nth = p.Nth;
}

MemoryController::AttackThrottler::~AttackThrottler() {
}

void MemoryController::AttackThrottler::update_BL_ACTs(uint64_t _th_id) {
  // called only when activating blacklisted row
  std::map<uint64_t,uint64_t>::iterator cur_iter = BL_ACTs.find(_th_id);

  if (cur_iter == BL_ACTs.end()) {
    BL_ACTs.insert(std::pair<uint64_t,uint64_t>(_th_id,1));
  }
  else {
    cur_iter->second = cur_iter->second + 1;
  }
}

void MemoryController::AttackThrottler::update_used_quota(uint64_t _th_id, bool _increment) {
  std::map<uint64_t,uint64_t>::iterator cur = used_quota.find(_th_id);
  if (cur == used_quota.end()) { // new
    if (_increment == false) // decrement and miss -> at the first reset.
      return;
    used_quota.insert(std::pair<uint64_t,uint64_t>(_th_id,1));
  }
  else { // existing
    if (_increment == true) { // increment
      cur->second = cur->second + 1;
    }
    else { // decrement
      if (cur->second == 0) {
        cur->second = 0;
      }
      else {
        cur->second = cur->second - 1;
      }
    }
  }
}

bool MemoryController::AttackThrottler::query(uint64_t _th_id,uint64_t _rank_num,uint64_t _bank_num,uint64_t _num) {
  float RHLI = float(BL_ACTs.find(_th_id)->second) / (Nth - blistTH);
  if (BL_ACTs.find(_th_id) == BL_ACTs.end())
    RHLI = 0;
  float max_quota = quota_base/(RHLI+0.02); // TODO - 0.0001 is the baseline.

  std::map<uint64_t,uint64_t>::iterator cur_iter = used_quota.find(_th_id);
  if (cur_iter == used_quota.end()) {
    return false;
  }

  if (cur_iter->second >= max_quota)
    return true;
  else 
    return false;
}

void MemoryController::AttackThrottler::reset() {
  std::cout << "AT Reset" << std::endl;
  BL_ACTs.clear();
  used_quota.clear();
}

void MemoryController::AttackThrottler::validate(bool _valid) {
  valid = _valid;
}

bool MemoryController::AttackThrottler::get_valid() {
  return valid;
}

void MemoryController::AttackThrottler::print_AttackThrottler() {
}

// "Hydra: Enabling Low-Overhead Mitigation of Row-Hammer at Ultra-Low Thresholds via Hybrid Tracking," ISCA, 2022
MemoryController::Hydra::Hydra()
  : hydra_gct_threshold(0), hydra_gct_num_entries(0),
    hydra_rcc_num_entries(0), hydra_rct_threshold(0),
    num_max_rows(0), page_sz_base_bit(0)
{
  group_base_bit = 0;
  num_rct_access = 0;
  num_gct_access = 0;
  num_rcc_access = 0;
}

MemoryController::Hydra::Hydra(uint64_t hydra_gct_threshold, uint64_t hydra_gct_num_entries, 
    uint64_t hydra_rcc_num_entries, uint64_t hydra_rct_threshold, uint64_t num_max_rows,
    uint64_t page_sz_base_bit)
  : hydra_gct_threshold(hydra_gct_threshold), hydra_gct_num_entries(hydra_gct_num_entries),
    hydra_rcc_num_entries(hydra_rcc_num_entries), hydra_rct_threshold(hydra_rct_threshold),
    num_max_rows(num_max_rows), page_sz_base_bit(page_sz_base_bit)
{
  uint32_t num_groups = uint32_t(num_max_rows / hydra_gct_num_entries);
  uint32_t log2 = 0;
  while (num_groups > 1)
  {
    log2 += 1;
    num_groups >>= 1;
  }

  cout << "[Hydra] Init\n";
  cout << "GCT_threshold: " << hydra_gct_threshold << endl;
  cout << "RCT_threshold: " << hydra_rct_threshold << endl;
  group_base_bit = log2;
  num_rct_access = 0;
  num_gct_access = 0;
  num_rcc_access = 0;
  for (uint64_t i = 0; i < hydra_gct_num_entries; ++i)
  {
    gct.insert(pair<uint64_t, uint64_t>(i, 0));
  }
}

MemoryController::Hydra::~Hydra()
{
}

int MemoryController::Hydra::activate(uint64_t curr_time, uint64_t address, uint32_t th_id)
{
  uint64_t row_addr = (address >> page_sz_base_bit);
  uint64_t gidx = get_gct_idx(address) % hydra_gct_num_entries;
  map<uint64_t, uint64_t>::iterator it = gct.find(gidx);

  if (it == gct.end())
  {
    // the target address is not found in GCT (first GCT access)
    gct.insert(pair<uint64_t, uint64_t>(gidx, 1));
    num_gct_access++;
    return 0;
  }
  else
  {
    if (it->second < hydra_gct_threshold)
    {
      it->second++;
      num_gct_access++;
      return 0;
    }
    else
    {
      int retval = 0;
      // group count reaches its threshold
      // check rcc/rct
      retval += query_rcc(address); // 1 if RCT access is needed
      retval += update_rct(address);  // 2 if refresh is needed
      return retval;
    }
  }

  return 0; // does not reach
}

int MemoryController::Hydra::query_rcc(uint64_t address)
{ // just use FIFO style cache
  uint64_t row_addr = (address >> page_sz_base_bit);
  int retval = 0;
  deque<uint64_t>::iterator it;
  for (it = rcc.begin(); it != rcc.end(); it++)
  {
    if (row_addr == *it)
    {
      break;
    }
  }

  if (it == rcc.end())
  {
    // rcc not found, then insert
    num_rct_access++;
    if (rcc.size() == hydra_rcc_num_entries)
    { // evict then insert
      rcc.pop_back();
      rcc.push_front(row_addr);
    }
    else
    { // just insert
      rcc.push_front(row_addr);
    }
    retval = 1; // get from rct
  }
  else
  {
    // RCC hit
    num_rcc_access++;
    rcc.erase(it);
    rcc.push_front(row_addr);
  }

  return retval;
}

int MemoryController::Hydra::update_rct(uint64_t address)
{
  uint64_t row_addr = (address >> page_sz_base_bit);
  map<uint64_t, uint64_t>::iterator rctit = rct.find(row_addr);
  int retval = 0;
  if (rctit != rct.end())
  { // rct hit
    (rctit->second)++;
    if (rctit->second >= hydra_rct_threshold)
    {
      retval = 2;
      rctit->second = 0;
    }
  }
  else
  { // rct miss
    rct.insert(pair<uint64_t, uint64_t>(row_addr, hydra_gct_threshold));
  }
  return retval;
}

uint64_t MemoryController::Hydra::get_gct_idx(uint64_t address)
{
  uint64_t page_num = (address >> page_sz_base_bit);
  return (page_num >> group_base_bit);  // naive hash function
}

void MemoryController::Hydra::reset()
{
  gct.clear();
  for (uint64_t i = 0; i < hydra_gct_num_entries; ++i)
  {
    gct.insert(pair<uint64_t, uint64_t>(i, 0));
  }
  rct.clear();
  rcc.clear();
}

// "Scalable and Secure Row-Swap: Efficient and Safe Row Hammer Mitigation in Memory Systems," HPCA, 2023
MemoryController::RRS::RRS(uint64_t rrs_threshold, uint64_t rrs_hrt_threshold, 
    uint32_t rit_num_entry, uint32_t hrt_num_entry, uint32_t rrs_num_tables)
    : rrs_threshold(rrs_threshold), rrs_hrt_threshold(rrs_hrt_threshold), 
    rit_num_entry(0), hrt_num_entry(hrt_num_entry), rrs_num_tables(rrs_num_tables)
{
  // we just use Misra-Gries table for counting
  entries = new Graphene_entry[hrt_num_entry];
  this->spcnt = 0;
  // we do not use lazy eviction (lazy eviction do not reduce the total number of row-swaps)
  // in our workloads, there is less time slot for lazy swap.
  this->lazy_eviction_count = 0;
}

MemoryController::RRS::~RRS() {
  // placeholder
}

void MemoryController::RRS::set_lazy_eviction_time(uint64_t interval) {
  this->last_lazy_eviction_time = 8192 * interval;
}

bool MemoryController::RRS::activate(uint64_t row_addr) {
  bool ret = false;
  ret = this->update_hrt(row_addr);
  return ret;
}

void MemoryController::RRS::periodic_refresh()
{
  for (uint32_t i = 0; i < hrt_num_entry; ++i) {
    entries[i].reset_address();
    entries[i].reset_count();
  }
}

void MemoryController::RRS::debug() {
  uint64_t count = 0;
  vector<pair<uint64_t, uint64_t>>::iterator it;
  for (uint32_t i = 0; i < hrt_num_entry; ++i) {
    if (entries[i].get_count() != 0) {
      cout << hex << entries[i].get_address() << ", " << dec << entries[i].get_count() << endl;
    }
  }
  cout << "Spill counter: " << this->spcnt << endl;
}

bool MemoryController::RRS::update_hrt(uint64_t row_addr) {
  int i;
  uint64_t dest;
  bool swap_flag = false;
  for (i = 0; i < hrt_num_entry; ++i) {
    if (entries[i].get_address() == row_addr) { // found, then increment count
      dest = i;
      entries[dest].increment_count();
      break;
    }
  }

  if (i == hrt_num_entry) { 
    // not found, replace one
    dest = hrt_num_entry - 1;
    entries[dest].set_address(row_addr);
    entries[dest].increment_count();
  }

  for (i = dest; i > 0; --i) { // sort
    if (entries[i].get_count() <= entries[i-1].get_count()) {
      break;
    }
    else {
      Graphene_entry temp;
      temp = entries[i];
      entries[i] = entries[i-1];
      entries[i-1] = temp;
      --dest;
    }
  }

  if ((entries[dest].get_count() % rrs_hrt_threshold) == 0
      && entries[dest].get_count() != 0) {
    // swap
    swap_flag = true;
  }

  return swap_flag;
}

bool MemoryController::RRS::lazy_eviction(uint64_t time, uint64_t interval) {
  uint64_t diff = time - last_lazy_eviction_time;
  bool swap_flag = false;
  if (diff > interval) {
    last_lazy_eviction_time = time;
    lazy_eviction_count += 1; // which one to lazy eviction
    swap_flag = true;
  }

  return swap_flag;
}

// "ABACuS: All-Bank Activation Counters for Scalable and Low Overhead RowHammer Mitigation," USENIX Security, 2024
MemoryController::ABACuS_entry::ABACuS_entry() 
    : row_id_(0), rac_(0), sav_(0) {
  // placeholder
}

MemoryController::ABACuS_entry::~ABACuS_entry() {
  // do nothing, placeholder
}

uint64_t MemoryController::ABACuS_entry::get_row_id() const {
  return row_id_;
}

void MemoryController::ABACuS_entry::set_row_id(uint64_t row_id) {
  row_id_ = row_id;
}

uint64_t MemoryController::ABACuS_entry::get_rac() const {
  return rac_;
}

void MemoryController::ABACuS_entry::set_rac(uint64_t rac) {
  rac_ = rac;
}

uint64_t MemoryController::ABACuS_entry::get_sav() const {
  return sav_;
}

void MemoryController::ABACuS_entry::set_sav(uint64_t sav) {
  sav_ = sav;
}

void MemoryController::ABACuS_entry::reset() {
  row_id_ = 0;
  rac_ = 0;
  sav_ = 0;
}

MemoryController::ABACuS::ABACuS(uint64_t n, uint64_t p, uint64_t r) 
    : num_entries_(n), spillover_counter_(0), prt_(p), rct_(r), 
    num_refresh_cycle_(0), num_periodic_refresh_(0) {
  entries.resize(num_entries_);
  num_preventive_refreshes_ = 0;
}

MemoryController::ABACuS::~ABACuS() {
  entries.clear();
}

int MemoryController::ABACuS::activate(uint64_t bank_id, uint64_t row_id) {
  int retval = 0;
  /*
   * retval:
   *  0: do nothing
   *  1: target rank/bank refresh
   *  2: refresh cycle 
   */
  // 
  auto it = std::find_if(entries.begin(), entries.end(), 
                        [row_id](const ABACuS_entry& entry) { 
                            return entry.get_row_id() == row_id;
                        });
  if (it != entries.end()) {
    // found
    uint64_t rac = it->get_rac();
    uint64_t sav = it->get_sav();

    if (sav & (1ULL << bank_id)) {
      // sav already set
      rac++;
      sav = (1ULL << bank_id); // zeroing out other sav bits
      it->set_rac(rac);
      it->set_sav(sav);
    }
    else {
      // first hit or other sav bit sets --> only update sav
      sav = sav + (1ULL << bank_id);
      it->set_sav(sav);
    }

    if (rac % prt_ == 0 && rac != 0) {
      // preventive refresh
      retval = 1;
      it->set_sav(0);
      it->set_rac(spillover_counter_ + 1);
    }
  }
  else {
    // not found, find the lowest rac entry
    auto min_rac_entry = std::min_element(entries.begin(), entries.end(),
                                          [](const ABACuS_entry& a, const ABACuS_entry& b) {
                                            return a.get_rac() < b.get_rac();
                                          });

    uint64_t min_rac = min_rac_entry->get_rac();
    if (min_rac == spillover_counter_) {
      // replace
      min_rac_entry->set_row_id(row_id);
      min_rac_entry->set_rac(spillover_counter_ + 1);
      min_rac_entry->set_sav(1ULL << bank_id);
    }
    else if (min_rac > spillover_counter_) {
      // increment spillover_counter
      spillover_counter_++;
    }
    else {
      cout << "[ABACuS] something wrong... rac: " << min_rac 
           << ", spillover: " << spillover_counter_ << "\n";
      cout << "[ABACuS] reset spillover counter!\n";
      this->refresh_cycle();
      retval = 2;
    }

    if (min_rac == prt_) {
      // preventive refresh
      retval = 1;
      min_rac_entry->set_sav(0);
      min_rac_entry->set_rac(spillover_counter_);
    }
  }

  if (spillover_counter_ == rct_) {
    // refresh cycle
    this->refresh_cycle();
    retval = 2;
  }
  return retval;
}

void MemoryController::ABACuS::refresh_cycle() {
  for (uint64_t i = 0; i < num_entries_; ++i) {
    entries[i].reset();
  }
  spillover_counter_ = 0;
  num_refresh_cycle_++;
}

void MemoryController::ABACuS::periodic_refresh() {
  for (uint64_t i = 0; i < num_entries_; ++i) {
    entries[i].reset();
  }
  num_periodic_refresh_++;
}

void MemoryController::ABACuS::preventive_refresh() {
  num_preventive_refreshes_++;
}

void MemoryController::ABACuS::debug(uint64_t rank) {
  cout << rank << "," << num_refresh_cycle_ << "," << num_preventive_refreshes_ << "\n";
}
