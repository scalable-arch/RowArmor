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

#include "PTSDirectory.h"
#include "PTSRBoL.h"
#include "PTSMemoryController.h"
#include "PTSXbar.h"
#include <assert.h>
#include <iomanip>
#include <cstdlib>

using namespace PinPthread;
using namespace std;

ostream & operator<<(ostream & output, rbol_status_type rst)
{
  switch (rst)
  {
    case rst_valid:        output << "rst_valid"; break;
    case rst_dirty:        output << "rst_dirty"; break;
    case rst_coming:       output << "rst_coming"; break;
    case rst_dirty_coming: output << "rst_dirty_coming"; break;
    case rst_invalid:      output << "rst_invalid"; break;
    default: break;
  }
  return output;
}


RBoL::RBoL(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_),
  set_lsb(get_param_uint64("set_lsb", 6)),
  rbol_set_lsb  (get_param_uint64("rbol_set_lsb", 10)),
  pred_set_lsb  (get_param_uint64("pred_set_lsb", 10)),
  rbol_entry_lsb(get_param_uint64("rbol_entry_lsb", 8)),
  max_rbol_entries_to_prefetch(get_param_uint64("max_rbol_entries_to_prefetch", 4)),
  num_rbol_sets (get_param_uint64("num_rbol_sets", 4)),
  num_rbol_ways (get_param_uint64("num_rbol_ways", 16)),
  num_pred_sets (get_param_uint64("num_pred_sets", 16)),
  num_pred_ways (get_param_uint64("num_pred_ways", 16)),
  to_dir_latest_time(0),
  to_dir_hit_t(get_param_uint64("to_dir_hit_t", 80)),
  num_rd_pred_access(0),
  num_wr_pred_access(0),
  num_rbol_hit(0),
  num_rbol_miss(0),
  num_rbol_evict(0),
  req_l(), rep_l(),
  use_rbol   (get_param_str("use_rbol") == "true"),
  do_not_check_neighbors(get_param_str("do_not_check_neighbors") == "true"),
  adaptive_rbol_line(get_param_str("adaptive_rbol_line") == "true"),
  max_prefetch_rbol_lines(get_param_uint64("max_prefetch_rbol_lines", 4))
{
  process_interval = get_param_uint64("process_interval", 80);
  pred_tags = vector< list<uint64_t> >(num_pred_sets);
  rbol_tags = vector< list< pair< uint64_t, rbol_status_type > > >(num_rbol_sets);
  neighbor_existence = new bool[max_prefetch_rbol_lines*2 + 1];
}


RBoL::~RBoL()
{
  delete [] neighbor_existence;
  if (num_rd_pred_access > 0)
  {
    cout << "  -- RBoL[" << num << "] : (rd_pred_acc, wr_pred_acc) = ("
         << num_rd_pred_access << ", " << num_wr_pred_access
         << "), (rbol_hit, rbol_miss, rbol_evict) = ("
         << num_rbol_hit << ", " << num_rbol_miss << ", " << num_rbol_evict
         << ")" << endl;
  }
}


void RBoL::show_state(uint64_t address)
{
  uint64_t addr_for_set = ((address >> mc->rank_interleave_base_bit) << mc->mc_interleave_base_bit) +
                            (address % (1 << mc->mc_interleave_base_bit));
  uint32_t rbol_set = (addr_for_set >> rbol_set_lsb) % num_rbol_sets;
  uint32_t pred_set = (address >> pred_set_lsb) % num_pred_sets;

  list< pair< uint64_t, rbol_status_type > >::iterator rbol_iter;
  for (rbol_iter = rbol_tags[rbol_set].begin(); rbol_iter != rbol_tags[rbol_set].end(); ++rbol_iter)
  {
    if ((rbol_iter->first >> set_lsb) == (address >> set_lsb))
    {
      cout << " -- RBoL[" << num << "] addr " << hex << address << dec << " exists" 
           << " " << (rbol_iter->second) << endl;
      break;
    }
  }
  list< uint64_t >::iterator pred_iter;
  for (pred_iter = pred_tags[pred_set].begin(); pred_iter != pred_tags[pred_set].end(); ++pred_iter)
  {
    if (((*pred_iter) >> rbol_entry_lsb) == (address >> rbol_entry_lsb))
    {
      cout << " -- Pred[" << num << "] addr " << hex << address << dec << " exists" << endl;
      break;
    }
  }
}


void RBoL::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  if (use_rbol == false)
  {
    mc->add_req_event(event_time + process_interval, local_event);
  }
  else
  {
    //if (num == 2) {cout << event_time << " Q " << hex << local_event->address << dec << " "; local_event->display();}
    geq->add_event(event_time, this);
    req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
  }
}


void RBoL::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  if (use_rbol == false)
  {
    directory->add_rep_event(event_time + process_interval, local_event);
  }
  else
  {
    //if (num == 2) {cout << event_time << " P " << hex << local_event->address << dec << " "; local_event->display();}
    geq->add_event(event_time, this);
    rep_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
  }
}


uint32_t RBoL::process_event(uint64_t curr_time)
{
  list<LocalQueueElement *>::iterator iter;
  multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    req_l.push_back(req_event_iter->second);
    ++req_event_iter;
  }
  req_event.erase(curr_time);

  multimap<uint64_t, LocalQueueElement *>::iterator rep_event_iter = rep_event.begin();

  while (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_l.push_back(rep_event_iter->second);
    ++rep_event_iter;
  }
  rep_event.erase(curr_time);


  if ((iter = rep_l.begin()) != rep_l.end())
  //for (iter = rep_l.begin(); iter != rep_l.end(); ++iter)
  {
    /*if ((*iter)->address >= search_addr && (*iter)->address < search_addr + 0x40)
    {
      cout << "  -- [" << setw(7) << curr_time << "] RBoLq[" << num << "] "; (*iter)->display();
      show_state((*iter)->address);
    }*/
    (*iter)->from.pop();
    // events returning from the memory controller
    uint64_t address  = (*iter)->address;
    uint64_t addr_for_set = ((address >> mc->rank_interleave_base_bit) << mc->mc_interleave_base_bit) +
                            (address % (1 << mc->mc_interleave_base_bit));
    uint32_t rbol_set = (addr_for_set >> rbol_set_lsb) % num_rbol_sets;

    list< pair< uint64_t, rbol_status_type > >::iterator rbol_iter;
    for (rbol_iter = rbol_tags[rbol_set].begin(); rbol_iter != rbol_tags[rbol_set].end(); ++rbol_iter)
    {
      if ((rbol_iter->first >> set_lsb) == (addr_for_set >> set_lsb))
      {
        //cout << num << " :: " << curr_time << " :: " << rbol_iter->second << " :: "
        //     << hex << rbol_iter->first << dec << endl;
        if (rbol_iter->second == rst_coming)
        {
          rbol_iter->second = rst_valid;
        }
        else if (rbol_iter->second == rst_dirty_coming)
        {
          rbol_iter->second = rst_dirty;
        }
        rbol_tags[rbol_set].push_back(*rbol_iter);
        rbol_tags[rbol_set].erase(rbol_iter);
        break;
      }
    }
    if ((*iter)->from.empty() == true)
    {
      delete (*iter);
    }
    else
    {
      send_to_dir(curr_time, *iter);
    }
    rep_l.erase(iter);
  }
  //rep_l.clear();


  //if ((iter = req_l.begin()) != req_l.end())
  for (iter = req_l.begin(); iter != req_l.end(); ++iter)
  {
    /*if ((*iter)->address >= search_addr && (*iter)->address < search_addr + 0x40)
    {
      cout << "  -- [" << setw(7) << curr_time << "] RBoLq[" << num << "] "; (*iter)->display();
      show_state((*iter)->address);
    }*/
    (*iter)->from.push(this);
    uint64_t address  = (*iter)->address;
    event_type type   = (*iter)->type;
    uint64_t addr_for_set = ((address >> mc->rank_interleave_base_bit) << mc->mc_interleave_base_bit) +
                            (address % (1 << mc->mc_interleave_base_bit));
    uint32_t pred_set = (addr_for_set >> pred_set_lsb) % num_pred_sets;
    uint32_t rbol_set = (addr_for_set >> rbol_set_lsb) % num_rbol_sets;
    bool is_read      = (type == et_rd_dir_info_req || type == et_rd_dir_info_rep ||
                         type == et_read || type == et_e_rd || type == et_s_rd);
    if (is_read)
    {
      num_rd_pred_access++;
    }
    else
    {
      num_wr_pred_access++;
    }

    list< uint64_t >::iterator pred_iter;
    bool check_n_update_rbol = false;
    for (pred_iter = pred_tags[pred_set].begin(); pred_iter != pred_tags[pred_set].end(); ++pred_iter)
    {
      if (((*pred_iter) >> rbol_entry_lsb) == (addr_for_set >> rbol_entry_lsb))
      {
        pred_tags[pred_set].push_back(*pred_iter);
        pred_tags[pred_set].erase(pred_iter);
        // predictor hit -- check RBoL. if not in RBoL, load it to RBoL
        check_n_update_rbol = true;
        break;
      }
    }
    if (check_n_update_rbol == false && pred_iter == pred_tags[pred_set].end())
    {
      pred_tags[pred_set].push_back(addr_for_set);
      if (pred_tags[pred_set].size() > num_pred_ways)
      {
        pred_tags[pred_set].erase(pred_tags[pred_set].begin());
      }
      if (do_not_check_neighbors == false)
      {
        // not in the predictor -- check if previous or next RBoL entries are in the predictor
        for (pred_iter = pred_tags[pred_set].begin(); pred_iter != pred_tags[pred_set].end(); ++pred_iter)
        {
          if (((*pred_iter) >> rbol_entry_lsb) - 1 == (addr_for_set >> rbol_entry_lsb) || 
              ((*pred_iter) >> rbol_entry_lsb) + 1 == (addr_for_set >> rbol_entry_lsb))
          {
            // check RBoL. if not in RBoL, load it to RBoL
            check_n_update_rbol = true;
            break;
          }
        }
      }
    }

    if (check_n_update_rbol == false)
    {
      // check RBoL. even if address is not in RBoL, do not load it to RBoL
      list< pair< uint64_t, rbol_status_type > >::iterator rbol_iter;
      for (rbol_iter = rbol_tags[rbol_set].begin(); rbol_iter != rbol_tags[rbol_set].end(); ++rbol_iter)
      {
        if ((rbol_iter->first >> set_lsb) == (addr_for_set >> set_lsb) &&
            (rbol_iter->second == rst_valid || rbol_iter->second == rst_dirty))
        {
          if (is_read == false)
          {
            rbol_iter->second = rst_dirty;
            if (type == et_s_rd_wr)
            {
              (*iter)->type  = et_s_rd;
              (*iter)->dummy = false;
              (*iter)->from.pop();
              send_to_dir(curr_time, *iter, true);
            }
            else
            {
              delete(*iter);
            }
          }
          else
          {
            (*iter)->dummy = false;
            (*iter)->from.pop();
            send_to_dir(curr_time, *iter, true);
          }
          rbol_tags[rbol_set].push_back(*rbol_iter);
          rbol_tags[rbol_set].erase(rbol_iter);
          rbol_iter = rbol_tags[rbol_set].begin();
          num_rbol_hit++;
          break;
        }
        else if ((rbol_iter->first >> set_lsb) == (addr_for_set >> set_lsb) &&
                 (rbol_iter->second == rst_coming || rbol_iter->second == rst_dirty_coming))
        {
          (*iter)->from.pop();
          geq->add_event(curr_time + process_interval, this);
          req_event.insert(pair<uint64_t, LocalQueueElement *>(curr_time + process_interval, *iter));
          break;
        }
      }
      if (rbol_iter == rbol_tags[rbol_set].end())
      {
        num_rbol_miss++;
        mc->add_req_event(curr_time + process_interval, *iter);
      }
    }
    else
    {
      int32_t prefetch_rbol_line_first = 0;
      int32_t prefetch_rbol_line_last  = 0;
      if (adaptive_rbol_line == true)
      {
        for (uint32_t i = 0; i <= 2*max_prefetch_rbol_lines; i++)
        {
          neighbor_existence[i] = false;
        }
        for (pred_iter = pred_tags[pred_set].begin(); pred_iter != pred_tags[pred_set].end(); ++pred_iter)
        {
          int64_t offset = (int64_t)((*pred_iter) >> rbol_entry_lsb) - 
                                     (addr_for_set >> rbol_entry_lsb) + max_prefetch_rbol_lines;
          if (offset >= 0 && offset <= 2*max_prefetch_rbol_lines)
          {
            neighbor_existence[offset] = true;
          }
        }
        for (uint32_t i = 1; i <= max_prefetch_rbol_lines; i++)
        {
          if (neighbor_existence[max_prefetch_rbol_lines+i] == true)
          {
            prefetch_rbol_line_first--;
          }
          else
          {
            break;
          }
        }
        for (uint32_t i = 1; i <= max_prefetch_rbol_lines; i++)
        {
          if (neighbor_existence[max_prefetch_rbol_lines-i] == true)
          {
            prefetch_rbol_line_last++;
          }
          else
          {
            break;
          }
        }
        //cout << prefetch_rbol_line_last << endl;
      }

      // check RBoL. if address is not in RBoL, load it to RBoL
      list< pair< uint64_t, rbol_status_type > >::iterator rbol_iter;
      for (rbol_iter = rbol_tags[rbol_set].begin(); rbol_iter != rbol_tags[rbol_set].end(); ++rbol_iter)
      {
        if ((rbol_iter->first >> set_lsb) == (addr_for_set >> set_lsb) &&
            (rbol_iter->second == rst_valid || rbol_iter->second == rst_dirty))
        {
          if (is_read == false)
          {
            rbol_iter->second = rst_dirty;
            if (type == et_s_rd_wr)
            {
              (*iter)->type  = et_s_rd;
              (*iter)->dummy = false;
              (*iter)->from.pop();
              send_to_dir(curr_time, *iter, true);
            }
            else
            {
              delete(*iter);
            }
          }
          else
          {
            (*iter)->dummy = false;
            (*iter)->from.pop();
            send_to_dir(curr_time, *iter, true);
          }
          rbol_tags[rbol_set].push_back(*rbol_iter);
          rbol_tags[rbol_set].erase(rbol_iter);
          rbol_iter = rbol_tags[rbol_set].begin();
          num_rbol_hit++;
          break;
        }
        else if ((rbol_iter->first >> set_lsb) == (addr_for_set >> set_lsb) &&
                 (rbol_iter->second == rst_coming || rbol_iter->second == rst_dirty_coming))
        {
          (*iter)->from.pop();
          geq->add_event(curr_time + process_interval, this);
          req_event.insert(pair<uint64_t, LocalQueueElement *>(curr_time + process_interval, *iter));
          break;
        }
      }
      if (rbol_iter == rbol_tags[rbol_set].end())
      {
        num_rbol_miss++;
        // evict if needed for a critical line
        if (rbol_tags[rbol_set].size() < num_rbol_ways)
        {
          if (is_read == false)
          {
            rbol_tags[rbol_set].push_back(pair<uint64_t, rbol_status_type>(addr_for_set, rst_dirty_coming));
            if (type == et_s_rd_wr)
            {
              (*iter)->type  = et_s_rd;
              (*iter)->dummy = false;
              (*iter)->from.pop();
              send_to_dir(curr_time, *iter);
            }
            else
            {
              delete (*iter);
            }
            LocalQueueElement * lqe = new LocalQueueElement(this, et_read, address);
            mc->add_req_event(curr_time + process_interval, lqe);
          }
          else
          {
            rbol_tags[rbol_set].push_back(pair<uint64_t, rbol_status_type>(addr_for_set, rst_coming));
            mc->add_req_event(curr_time + process_interval, *iter);
          }
        }
        else
        {
          if ((rbol_tags[rbol_set].begin())->second == rst_coming ||
              (rbol_tags[rbol_set].begin())->second == rst_dirty_coming)
          {
            (*iter)->from.pop();
            geq->add_event(curr_time + process_interval, this);
            req_event.insert(pair<uint64_t, LocalQueueElement *>(curr_time + process_interval, *iter));
          }
          else
          {
            if ((rbol_tags[rbol_set].begin())->second == rst_dirty)
            {
              LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, (rbol_tags[rbol_set].begin())->first);
              mc->add_req_event(curr_time + process_interval, lqe);
              num_rbol_evict++;
            }
            rbol_tags[rbol_set].erase(rbol_tags[rbol_set].begin());
            if (is_read == false)
            {
              rbol_tags[rbol_set].push_back(pair<uint64_t, rbol_status_type>(addr_for_set, rst_dirty_coming));
              if (type == et_s_rd_wr)
              {
                (*iter)->type  = et_s_rd;
                (*iter)->dummy = false;
                (*iter)->from.pop();
                send_to_dir(curr_time, *iter);
              }
              else
              {
                delete (*iter);
              }
              LocalQueueElement * lqe = new LocalQueueElement(this, et_read, address);
              mc->add_req_event(curr_time + process_interval, lqe);
            }
            else
            {
              rbol_tags[rbol_set].push_back(pair<uint64_t, rbol_status_type>(addr_for_set, rst_coming));
              mc->add_req_event(curr_time + process_interval, *iter);
            }
          }
        }
        // evict if needed for non-critical lines
        for (int h = prefetch_rbol_line_first; h <= prefetch_rbol_line_last; h++)
        {
        for (int idx = (h == 0 ? 1 : 0); idx < (1 << (rbol_entry_lsb - set_lsb)); idx++)
        {
          uint64_t ld_addr = ((address >> rbol_entry_lsb) << rbol_entry_lsb) +
                             (address + idx*(1 << set_lsb))%(1 << rbol_entry_lsb);
          uint64_t addr_for_set = ((ld_addr >> mc->rank_interleave_base_bit) << mc->mc_interleave_base_bit) +
                                  (ld_addr % (1 << mc->mc_interleave_base_bit)) +
                                  ((int64_t)h * (1 << rbol_entry_lsb)); 
          uint32_t pred_set = (addr_for_set >> pred_set_lsb) % num_pred_sets;
          uint32_t rbol_set = (addr_for_set >> rbol_set_lsb) % num_rbol_sets;
          list< pair< uint64_t, rbol_status_type > >::iterator rbol_iter;
          // if ld_addr in RBoL, skip
          for (rbol_iter = rbol_tags[rbol_set].begin(); rbol_iter != rbol_tags[rbol_set].end(); ++rbol_iter)
          {
            if ((rbol_iter->first >> set_lsb) == (addr_for_set >> set_lsb))
            {
              break;
            }
          }
          if (rbol_iter != rbol_tags[rbol_set].end())
          {
            continue;
          }

          pred_tags[pred_set].push_back(addr_for_set);
          if (pred_tags[pred_set].size() > num_pred_ways)
          {
            pred_tags[pred_set].erase(pred_tags[pred_set].begin());
          }
          uint64_t addr_for_prefetch = (addr_for_set % (1 << mc->mc_interleave_base_bit)) +
                                       (((address % (1 << mc->rank_interleave_base_bit)) >> mc->mc_interleave_base_bit) << mc->mc_interleave_base_bit) +
                                       ((addr_for_set >> mc->mc_interleave_base_bit) << mc->rank_interleave_base_bit);
          if (rbol_tags[rbol_set].size() < num_rbol_ways)
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_read, addr_for_prefetch);
            rbol_tags[rbol_set].push_back(pair<uint64_t, rbol_status_type>(addr_for_set, rst_coming));
            mc->add_req_event(curr_time + process_interval, lqe);
          }
          else
          {
            rbol_iter = rbol_tags[rbol_set].begin();
            if (rbol_iter->second != rst_coming && rbol_iter->second != rst_dirty_coming)
            {
              if (rbol_iter->second == rst_dirty)
              {  
                LocalQueueElement * dtlqe = new LocalQueueElement(this, et_evict, rbol_iter->first);
                mc->add_req_event(curr_time + process_interval, dtlqe);
              }
              rbol_tags[rbol_set].erase(rbol_iter);
              LocalQueueElement * lqe = new LocalQueueElement(this, et_read, addr_for_prefetch);
              rbol_tags[rbol_set].push_back(pair<uint64_t, rbol_status_type>(addr_for_set, rst_coming));
              mc->add_req_event(curr_time + process_interval, lqe);
            }
          }
        }
        }
      }
    }
    //req_l.erase(iter);
  }
  req_l.clear();

  if (req_l.empty() == false || rep_l.empty() == false)
  {
    geq->add_event(curr_time + process_interval, this);
  }
  return 0;
}


void RBoL::send_to_dir(uint64_t curr_time, LocalQueueElement * lqe, bool hit)
{
  if (to_dir_latest_time > curr_time)
  {
    directory->add_rep_event(to_dir_latest_time + (hit == true ? to_dir_hit_t : process_interval), lqe);
    to_dir_latest_time += process_interval;
  }
  else
  {
    directory->add_rep_event(curr_time + (hit == true ? to_dir_hit_t : process_interval), lqe);
    to_dir_latest_time = curr_time + process_interval;
  }
}

