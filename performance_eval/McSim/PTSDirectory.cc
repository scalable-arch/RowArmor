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
#include "PTSCache.h"
#include "PTSRBoL.h"
#include "PTSMemoryController.h"
#include "PTSXbar.h"
#include <assert.h>
#include <iomanip>

using namespace PinPthread;
using namespace std;


extern ostream & operator << (ostream & output, coherence_state_type cs);
extern ostream & operator << (ostream & output, component_type ct);
extern ostream & operator << (ostream & output, event_type et);


Directory::Directory(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_),
  set_lsb       (get_param_uint64("set_lsb", 6)),
  num_sets      (get_param_uint64("num_sets", 16)),
  num_ways      (get_param_uint64("num_ways", 4)),
  dir_to_mc_t   (get_param_uint64("to_mc_t", 450)),
  dir_to_l2_t   (get_param_uint64("to_l2_t", 140)),
  dir_to_xbar_t (get_param_uint64("to_xbar_t", 350)),
  num_flits_per_packet(get_param_uint64("num_flits_per_packet", 1)),
  num_nack(0), num_bypass(0),
  num_i_to_tr(0), num_e_to_tr(0), num_s_to_tr(0), num_o_to_tr(0), num_m_to_tr(0),
  num_m_to_i(0),
  num_tr_to_i(0), num_tr_to_e(0), num_tr_to_s(0), num_tr_to_o(0), num_tr_to_m(0),
  num_evict(0), num_invalidate(0), num_from_mc(0),
  num_dir_cache_access(0), num_dir_cache_miss(0), num_dir_cache_retry(0), num_dir_evict(0)
{
  process_interval        = get_param_uint64("process_interval", 50);
  has_directory_cache     = (get_param_str("has_directory_cache") == "true");
  use_limitless           = (get_param_str("use_limitless") == "true");
  limitless_broadcast_threshold = get_param_uint64("limitless_broadcast_threshold", 4);
  is_shared_llc = mcsim->pts->get_param_str("is_shared_llc") == "true" ? true : false;
//  is_shared_llc           = (get_param_str("is_shared_llc") == "true");

  dir = std::map<uint64_t, DirEntry>();
  dir_cache = std::vector< std::list<uint64_t> >(num_sets);
//  num_sharer_histogram = std::vector<uint64_t>(mcsim->l2s.size()+1, 0);
}


Directory::~Directory()
{
  for (std::map<uint64_t, DirEntry>::iterator iter = dir.begin(); iter != dir.end(); ++iter)
  {
//    num_sharer_histogram[iter->second.num_sharer]++;
  }

  if (num_i_to_tr > 0)
  {
    cout << "  -- Dir [" << setw(3) << num << "] : (i->tr, e->tr, s->tr, o->tr, m->tr, m->i, tr->i, tr->e, tr->s, tr->o, tr->m) = ("
         << num_i_to_tr << ", " << num_e_to_tr << ", " << num_s_to_tr << ", " << num_o_to_tr << ", " << num_m_to_tr << ", "
         << num_m_to_i << ", "
         << num_tr_to_i << ", " << num_tr_to_e << ", " << num_tr_to_s << ", " << num_tr_to_o << ", " << num_tr_to_m << ")" << endl;
    cout << "  -- Dir [" << setw(3) << num << "] : (nack, bypass, evict, invalidate, from_mc, dir_acc, dir_$_miss, dir_$_retry, dir_$_evict) = ("
         << num_nack << ", " << num_bypass << ", " << num_evict << ", "
         << num_invalidate << ", " << num_from_mc << ", "
         << num_dir_cache_access << ", " << num_dir_cache_miss << ", "
         << num_dir_cache_retry << ", " << num_dir_evict << "), ";
//    for (unsigned int i = 1; i < num_sharer_histogram.size(); i++)
//    {
//      cout << num_sharer_histogram[i] << ", ";
//    }
    cout << endl;
  }
}


void Directory::show_state(uint64_t address)
{
  uint64_t dir_entry = (address >> set_lsb);
 
  if (dir.find(dir_entry) != dir.end())
  {
    cout << "  -- DIR[" << num << "] : " << dir[dir_entry].type;
    if (dir[dir_entry].not_in_dc == true)
    {
      cout << ", not_in_dc";
    }
    for(std::set<Component *>::iterator iter = dir[dir_entry].sharedl2.begin();
        iter != dir[dir_entry].sharedl2.end(); ++iter)
    {
      cout << ", (" << (*iter)->type << ", " << (*iter)->num << ") ";
    }
    if (dir[dir_entry].pending != NULL)
    {
      dir[dir_entry].pending->display();
    }
    else
    {
      cout << endl;
    }
  }
}


void Directory::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void Directory::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);

  if (local_event->type == et_rd_dir_info_req)
  {
    req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
  }
  else
  {
    rep_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
  }
}


uint32_t Directory::process_event(uint64_t curr_time)
{
  multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();
  multimap<uint64_t, LocalQueueElement *>::iterator rep_event_iter = rep_event.begin();

  LocalQueueElement * rep_lqe = NULL;
  LocalQueueElement * req_lqe = NULL;
  num_dir_cache_access++;
  // event -> queue
  if (rep_q.empty() == false)
  {
    rep_lqe = rep_q.front();
    rep_q.pop();
  }
  else if (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_lqe = rep_event_iter->second;
    ++rep_event_iter;
  }

  while (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_q.push(rep_event_iter->second);
    ++rep_event_iter;
  }
  rep_event.erase(curr_time);


  if (rep_lqe != NULL)
  {
    // req event won't be processed at this time
  }
  else if (req_q.empty() == false)
  {
    req_lqe = req_q.front();
    req_q.pop();
  }
  else if (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    req_lqe = req_event_iter->second;
    ++req_event_iter;
  }
  else
  {
    display();  req_event_iter->second->display();  geq->display();  ASSERTX(0);
  }

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    req_q.push(req_event_iter->second);
    ++req_event_iter;
  }
  req_event.erase(curr_time);

  if (rep_q.empty() == false || req_q.empty() == false)
  {
    geq->add_event(curr_time + process_interval, this);
  }


  if (rep_lqe != NULL)
  {
    if (rep_lqe->address >= search_addr && rep_lqe->address < search_addr + 0x40)
    {
      cout << "  -- [" << setw(7) << curr_time << "] DIRp "; rep_lqe->display();
      show_state(rep_lqe->address);
    }

    // Directory -> L2 or Crossbar
    // reply events have higher priority than request events
    uint64_t address   = rep_lqe->address;
    uint64_t dir_entry = (address >> set_lsb);
    uint32_t set       = dir_entry % num_sets;
    event_type etype = rep_lqe->type;

    if (etype == et_evict || etype == et_evict_owned || etype == et_rd_dir_info_rep)
    {
      if (dir.find(dir_entry) == dir.end())
      {
        delete rep_lqe;
        return 0;
      }
      DirEntry & d_entry = dir[dir_entry];

      if (has_directory_cache == true)
      {
        // check if the entry exists in the directory cache.
        // evict an oldest directory cache line to the memory 
        list<uint64_t> & curr_set = dir_cache[set];
        list<uint64_t>::iterator iter;
        bool in_dir_cache = false;

        for (iter = curr_set.begin(); iter != curr_set.end(); ++iter)
        {
          if (*iter == dir_entry)
          {
            // directory cache has the entry
            in_dir_cache = true;
            curr_set.erase(iter);
            curr_set.push_front(dir_entry);
            iter = curr_set.begin();

            if (etype == et_rd_dir_info_rep)
            {
              d_entry.not_in_dc = false;
              delete rep_lqe;
              return 0;
            }
            else if (d_entry.not_in_dc == true)
            {
              // the entry is on its way from memory
              this->add_rep_event(curr_time + 100 * process_interval, rep_lqe);
              return 0;
            }
            break;
          }
        }

        if (in_dir_cache == false)
        {
          num_dir_cache_miss++;
          if (curr_set.size() == num_ways)
          {
            for (iter = curr_set.begin(); iter != curr_set.end(); ++iter)
            {
              coherence_state_type ctype = dir[*iter].type;
            
              if (ctype == cs_tr_to_s || ctype == cs_tr_to_m || ctype == cs_tr_to_i ||
                  ctype == cs_tr_to_e || ctype == cs_m_to_o || ctype == cs_tr_to_o ||
                  dir[*iter].pending != NULL || dir[*iter].not_in_dc == true)
              {
                continue;
              }
              else
              {
                // evict
                num_dir_evict++;
                LocalQueueElement * lqe = new LocalQueueElement(this, et_dir_evict, address);
                lqe->th_id = req_lqe->th_id;
                if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, lqe, this);
                else memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe, this);

                curr_set.erase(iter);
                curr_set.push_front(dir_entry);
                d_entry.not_in_dc = true;
                iter = curr_set.begin();
                break;
              }
            }

            if (iter == curr_set.end())
            {
              num_dir_cache_retry++;
              // not able to perform an action -- try later
              this->add_rep_event(curr_time + 2 * process_interval, rep_lqe);
              return 0;
            }
          }
          else
          {
            curr_set.push_front(dir_entry);
            d_entry.not_in_dc = true;
          }

          // get the directory information from the memory controller
          LocalQueueElement * lqe = new LocalQueueElement(this, et_rd_dir_info_rep, address);
          lqe->th_id = req_lqe->th_id;
          if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, lqe, this);
          else memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe, this);

          //this->add_rep_event(curr_time + 2 * process_interval, rep_lqe);
          num_dir_cache_retry++;
          //return 0;
        }
      }

      num_evict++;

      if (d_entry.type == cs_modified || d_entry.type == cs_m_to_o)
      {
        if (d_entry.sharedl2.size() > 1)
        {
          cout << "sharedl2.size() = " << d_entry.sharedl2.size() << endl;
          rep_lqe->display();  geq->display();  ASSERTX(0);
        }
        if (d_entry.sharedl2.find(rep_lqe->from.top()) == d_entry.sharedl2.end())
        {
          delete rep_lqe;
        }
        else
        {
          if (d_entry.type == cs_modified)
          {
            num_m_to_i++;
//            num_sharer_histogram[d_entry.num_sharer]++;
            dir.erase(dir_entry);
            remove_directory_cache_entry(set, dir_entry);
          }
          else
          {
            d_entry.sharedl2.erase(rep_lqe->from.top());
          }
          rep_lqe->from.push(this);
          if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, rep_lqe, this);
          else memorycontroller->add_req_event(curr_time + dir_to_mc_t, rep_lqe, this);
        }
      }
      else if (d_entry.type == cs_tr_to_s || d_entry.type == cs_tr_to_o)
      {
        d_entry.sharedl2.erase(rep_lqe->from.top());
        delete rep_lqe;
      }
      else if (d_entry.type == cs_tr_to_m)
      {
        delete rep_lqe;
      }
      else
      {
        if (d_entry.type == cs_owned && etype == et_evict_owned)
        { // gajh: o->s due to an evict
          d_entry.type = cs_shared;
          LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, address);
          lqe->th_id = rep_lqe->th_id;
          if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, lqe, this);
          else memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe, this);
        }
        d_entry.sharedl2.erase(rep_lqe->from.top());
        
        if (d_entry.sharedl2.empty() == true)
        {
//          num_sharer_histogram[d_entry.num_sharer]++;
          dir.erase(dir_entry);
          remove_directory_cache_entry(set, dir_entry);
        }
        delete rep_lqe;
      }
    }
    else if (etype == et_e_to_i || etype == et_e_to_m)
    {
      if (dir.find(dir_entry) == dir.end())
      {
        delete rep_lqe;
      }
      else
      {
        DirEntry & d_entry = dir[dir_entry];

        if (d_entry.type != cs_tr_to_m)
        {
          cout << "d_entry.type = " << d_entry.type << endl;
          rep_lqe->display();  geq->display();  ASSERTX(0);
        }
        if (etype == et_e_to_i)
        {
          num_tr_to_i++;
//          num_sharer_histogram[d_entry.num_sharer]++;
          dir.erase(dir_entry);
          remove_directory_cache_entry(set, dir_entry);
        }
        else
        {
          num_tr_to_m++;
          d_entry.type = cs_modified;
          d_entry.pending = NULL;
        }
        delete rep_lqe;
      }
    }
    else if (etype == et_invalidate || etype == et_invalidate_nd)
    {
      if (dir.find(dir_entry) == dir.end())
      {
        delete rep_lqe;
      }
      else
      {
        DirEntry & d_entry = dir[dir_entry];

        if (d_entry.type != cs_tr_to_m)
        {
          cout << "d_entry.type = " << d_entry.type << endl;
          rep_lqe->display();  geq->display();  ASSERTX(0);
        }
        d_entry.got_cl = (etype == et_invalidate) ? true : d_entry.got_cl;
        // remove sharedl2
        rep_lqe->from.pop();
        d_entry.sharedl2.erase(rep_lqe->from.top());
        ASSERTX(d_entry.pending);
        if (d_entry.sharedl2.empty() == true)
        {
          d_entry.sharedl2.insert(d_entry.pending->from.top());

          if (d_entry.got_cl == true)
          {
            num_tr_to_m++;
            d_entry.type   = cs_modified;
            add_event_to_UL(curr_time, d_entry.pending, true);
            d_entry.got_cl = false;
          }
          else
          {
            // resume the pending event
            d_entry.pending->type = et_e_rd;
            d_entry.pending->from.push(this);
            if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, d_entry.pending, this);
            else memorycontroller->add_req_event(curr_time + dir_to_mc_t, d_entry.pending, this);
          }
          d_entry.pending = NULL;
        }
        delete rep_lqe;
      }
    }
    else if (etype == et_e_to_s_nd || etype == et_s_to_s_nd || etype == et_dir_rd_nd)
    {
      if (dir.find(dir_entry) == dir.end() || 
          (dir[dir_entry].type != cs_tr_to_s && dir[dir_entry].type != cs_m_to_o))
      {
        cout << "etype = " << etype << endl;
        rep_lqe->display();  geq->display();  ASSERTX(0);
      }
      DirEntry & d_entry = dir[dir_entry];

      // change directory entry state to cs_shared -- gajh: do we need something
      // special for the case of et_dir_rd_nd?
      num_tr_to_s++;
      num_nack++;
      d_entry.type          = cs_shared;
      d_entry.pending->type = et_nack;
      add_event_to_ULpp(curr_time, d_entry.pending, false);
      d_entry.pending = NULL;
      if (d_entry.sharedl2.empty() == true)
      {
//        num_sharer_histogram[d_entry.num_sharer]++;
        dir.erase(dir_entry);
        remove_directory_cache_entry(set, dir_entry);
      }
      delete rep_lqe;
    }
    else if (etype == et_o_to_o_nd)
    {
      if (dir.find(dir_entry) == dir.end() || dir[dir_entry].type != cs_tr_to_o)
      {
        cout << "etype = " << etype << endl;
        rep_lqe->display();  geq->display();  ASSERTX(0);
      }
      DirEntry & d_entry = dir[dir_entry];

      // change directory entry state to cs_shared -- gajh: do we need something
      // special for the case of et_dir_rd_nd?
      num_tr_to_o++;
      num_nack++;
      d_entry.type          = cs_owned;
      d_entry.pending->type = et_nack;
      add_event_to_ULpp(curr_time, d_entry.pending, false);
      d_entry.pending = NULL;
      if (d_entry.sharedl2.empty() == true)
      {
//        num_sharer_histogram[d_entry.num_sharer]++;
        dir.erase(dir_entry);
        remove_directory_cache_entry(set, dir_entry);
      }
      delete rep_lqe;
    }
    else if (etype == et_e_to_s || etype == et_s_to_s)
    {
      if (dir.find(dir_entry) == dir.end() || dir[dir_entry].type != cs_tr_to_s)
      {
        cout << "etype = " << etype << endl;
        rep_lqe->display();  geq->display();  ASSERTX(0);
      }
      DirEntry & d_entry = dir[dir_entry];

      // change directory entry state to cs_shared
      num_tr_to_s++;
      d_entry.type = cs_shared;

      // resume the pending event
      d_entry.sharedl2.insert(d_entry.pending->from.top());
      d_entry.num_sharer = (d_entry.sharedl2.size()>d_entry.num_sharer) ? d_entry.sharedl2.size():d_entry.num_sharer;
      d_entry.pending->type = et_s_rd;
      add_event_to_UL(curr_time, d_entry.pending, true);
      d_entry.pending = NULL;
      delete rep_lqe;
    }
    else if (etype == et_o_to_o)
    {
      if (dir.find(dir_entry) == dir.end() || dir[dir_entry].type != cs_tr_to_o)
      {
        cout << "etype = " << etype << endl;
        rep_lqe->display();  geq->display();  ASSERTX(0);
      }
      DirEntry & d_entry = dir[dir_entry];

      // change directory entry state to cs_shared
      num_tr_to_o++;
      d_entry.type = cs_owned;

      // resume the pending event
      d_entry.sharedl2.insert(d_entry.pending->from.top());
      d_entry.num_sharer = (d_entry.sharedl2.size()>d_entry.num_sharer) ? d_entry.sharedl2.size():d_entry.num_sharer;
      d_entry.pending->type = et_s_rd;
      add_event_to_UL(curr_time, d_entry.pending, true);
      d_entry.pending = NULL;
      delete rep_lqe;
    }
    else if (etype == et_dir_rd)
    {
      if (dir.find(dir_entry) == dir.end() || dir[dir_entry].type != cs_m_to_o)
      {
        cout << "etype = " << etype << endl;
        rep_lqe->display();  geq->display();  ASSERTX(0);
      }
      DirEntry & d_entry = dir[dir_entry];

      // change directory entry state to cs_owned
      num_tr_to_o++;
      d_entry.type = cs_owned;

      // resume the pending event
      
      // gajh -- below will be done when an owned state L2 is evicted
      //LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, address);
      //lqe->th_id = rep_lqe->th_id;
      //memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe);

      d_entry.sharedl2.insert(d_entry.pending->from.top());
      d_entry.num_sharer = (d_entry.sharedl2.size()>d_entry.num_sharer) ? d_entry.sharedl2.size():d_entry.num_sharer;
      d_entry.pending->type = et_s_rd;
      add_event_to_UL(curr_time, d_entry.pending, true);
      d_entry.pending = NULL;
      delete rep_lqe;
    }
    else
    {
      num_from_mc++;
      rep_lqe->from.pop();

      if (dir.find(dir_entry) != dir.end())
      {
        if (dir[dir_entry].type == cs_tr_to_e && rep_lqe->type == et_e_rd)
        {
          num_tr_to_e++;
          dir[dir_entry].type = cs_exclusive;
        }
        else if (dir[dir_entry].type == cs_tr_to_m && rep_lqe->type == et_e_rd)
        {
          num_tr_to_m++;
          dir[dir_entry].type = cs_modified;
          rep_lqe->type = et_write;
        }
      }
      add_event_to_ULpp(curr_time, rep_lqe, true);
    }
  }
  else if (req_lqe != NULL)
  {
//    if (req_lqe->address >= search_addr && req_lqe->address < search_addr + 0x40)
//    {
//      cout << "  -- [" << setw(7) << curr_time << "] DIRq "; req_lqe->display();
//      show_state(req_lqe->address);
//    }
    uint64_t address   = req_lqe->address;
    uint64_t dir_entry = (address >> set_lsb);
    uint32_t set       = dir_entry % num_sets;
    event_type etype   = req_lqe->type;

    if (dir.find(dir_entry) == dir.end())
    {
      if (has_directory_cache == true)
      {
        // evict an oldest directory cache line to the memory 
        list<uint64_t> & curr_set = dir_cache[set];
        list<uint64_t>::iterator iter;
        num_dir_cache_miss++;

        if (curr_set.size() == num_ways)
        {
          for (iter = curr_set.begin(); iter != curr_set.end(); ++iter)
          {
            coherence_state_type ctype = dir[*iter].type;
            
            if (ctype == cs_tr_to_s || ctype == cs_tr_to_m || ctype == cs_tr_to_i ||
                ctype == cs_tr_to_e || ctype == cs_m_to_o || ctype == cs_tr_to_o ||
                dir[*iter].pending != NULL || dir[*iter].not_in_dc == true)
            {
              continue;
            }
            else
            { // evict
              num_dir_evict++;
              LocalQueueElement * lqe = new LocalQueueElement(this, et_dir_evict, address);
              lqe->th_id = req_lqe->th_id;
              if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, lqe, this);
              else memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe, this);
              curr_set.erase(iter);
              curr_set.push_front(dir_entry);
              iter = curr_set.begin();
              break;
            }
          }

          if (iter == curr_set.end())
          { // not possible to evict -- nack!
            num_nack++;
            req_lqe->type = et_nack;
            add_event_to_ULpp(curr_time, req_lqe, false);
            return 0;
          }
        }
        else
        {
          curr_set.push_front(dir_entry);
        }
      }

      // add the entry to the directory
      // and load the directory information from the memory if there is a directory cache
      dir.insert(pair<uint64_t, DirEntry>(dir_entry, DirEntry())); 
      dir[dir_entry].sharedl2.insert(req_lqe->from.top());
      dir[dir_entry].num_sharer = 1;

      num_i_to_tr++;
      dir[dir_entry].type = (etype == et_read) ? cs_tr_to_e : cs_tr_to_m;
      req_lqe->type = et_e_rd;
      req_lqe->from.push(this);
      if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, req_lqe, this);
      else memorycontroller->add_req_event(curr_time + dir_to_mc_t, req_lqe, this);
    }
    else
    {
      DirEntry & d_entry = dir[dir_entry];

      if (has_directory_cache == true)
      {
        // check if the entry exists in the directory cache.
        // evict an oldest directory cache line to the memory 
        list<uint64_t> & curr_set = dir_cache[set];
        list<uint64_t>::iterator iter;

        for (iter = curr_set.begin(); iter != curr_set.end(); ++iter)
        {
          if (*iter == dir_entry)
          {
            // directory cache has the entry
            curr_set.erase(iter);
            curr_set.push_front(dir_entry);
            iter = curr_set.begin();

            if (etype == et_rd_dir_info_req)
            {
              // replace pending into req_lqe
              delete req_lqe;
              req_lqe = d_entry.pending;
              d_entry.pending   = NULL;
              d_entry.not_in_dc = false;
              etype   = req_lqe->type;
            }
            else if (d_entry.pending != NULL)
            {
              // we are getting directory info from memory but it is not arrived yet -- nack!
              num_nack++;
              req_lqe->type = et_nack;
              add_event_to_ULpp(curr_time, req_lqe, false);
              return 0;
            }

            break;
          }
        }

        if (iter == curr_set.end())
        {
          num_dir_cache_miss++;
          if (etype == et_rd_dir_info_req)
          {
            // directory cache line was evicted while the exact line info
            // is extracted from the memory controller -- quite a corner case
            delete req_lqe;
            ASSERTX(0);
          }
          if (curr_set.size() == num_ways)
          {
            for (iter = curr_set.begin(); iter != curr_set.end(); ++iter)
            {
              coherence_state_type ctype = dir[*iter].type;
            
              if (ctype == cs_tr_to_s || ctype == cs_tr_to_m || ctype == cs_tr_to_i ||
                  ctype == cs_tr_to_e || ctype == cs_m_to_o || ctype == cs_tr_to_o ||
                  dir[*iter].pending != NULL || dir[*iter].not_in_dc == true)
              {
                continue;
              }
              else
              { // evict
                num_dir_evict++;
                LocalQueueElement * lqe = new LocalQueueElement(this, et_dir_evict, address);
                lqe->th_id = req_lqe->th_id;
                if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, lqe, this);
                else memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe, this);
                curr_set.erase(iter);
                curr_set.push_front(dir_entry);
                iter = curr_set.begin();
                break;
              }
            }

            if (iter == curr_set.end())
            {
              // not possible to evict -- nack!
              num_nack++;
              req_lqe->type = et_nack;
              add_event_to_ULpp(curr_time, req_lqe, false);
              return 0;
            }
          }
          else
          {
            curr_set.push_front(dir_entry);
          }

          // get the directory information from the memory controller
          d_entry.pending = req_lqe;
          d_entry.not_in_dc = true;

          LocalQueueElement * lqe = new LocalQueueElement(this, et_rd_dir_info_req, address);
          lqe->th_id = req_lqe->th_id;
          if (is_shared_llc) crossbar->add_dirrq_event(curr_time + dir_to_xbar_t, lqe, this);
          else memorycontroller->add_req_event(curr_time + dir_to_mc_t, lqe, this);

          return 0;
        }
      }

      // replace the entry in the directory
      coherence_state_type ctype = d_entry.type;

      if (ctype == cs_tr_to_s || ctype == cs_tr_to_m || ctype == cs_tr_to_i ||
          ctype == cs_tr_to_e || ctype == cs_m_to_o  || ctype == cs_tr_to_o)
      {
        num_nack++;
        req_lqe->type = et_nack;
        add_event_to_ULpp(curr_time, req_lqe, false);
      }
      else if (etype == et_read)
      {
        if (d_entry.sharedl2.find(req_lqe->from.top()) != d_entry.sharedl2.end())
        {
          // TODO -- currently miss after miss is treated as NACK.
          // We can do some optimization here since the L2 already has data.
          num_nack++;
          req_lqe->type = et_nack;
          add_event_to_ULpp(curr_time, req_lqe, false);
        }
        else if (ctype == cs_exclusive || ctype == cs_modified)
        {
          if (d_entry.sharedl2.size() != 1)
          {
            cout << "d_entry.sharedl2.size() = " << d_entry.sharedl2.size() << endl;
            req_lqe->display();  geq->display();  ASSERTX(0);
          }
          // hold the request in the directory
          d_entry.pending = req_lqe;
          // move the state of the directory entry into pending state
          d_entry.type    = (ctype == cs_exclusive) ? cs_tr_to_s : cs_m_to_o;
          (ctype == cs_exclusive) ? num_e_to_tr++ : num_m_to_tr++;
          // generate a request to the L2 to move the state from modified to shared
          LocalQueueElement * lqe = new LocalQueueElement(this, 
              (ctype == cs_exclusive) ? et_e_to_s : et_dir_rd, address);
          lqe->th_id = req_lqe->th_id;
          add_event_to_UL(curr_time, *(d_entry.sharedl2.begin()), lqe);
        }
        else if (ctype == cs_shared)
        {
          // hold the request in the directory and get data from a L2 cache
          // that added the cache line most recently.
          d_entry.pending = req_lqe;
          num_s_to_tr++;
          d_entry.type    = cs_tr_to_s;
          LocalQueueElement * lqe = new LocalQueueElement(this, et_s_to_s, address);
          lqe->th_id = req_lqe->th_id;
          add_event_to_UL(curr_time, *(d_entry.sharedl2.begin()), lqe);
        }
        else if (ctype == cs_owned)
        {
          // hold the request in the directory and get data from a L2 cache
          // that added the cache line most recently.
          d_entry.pending = req_lqe;
          num_s_to_tr++;
          d_entry.type    = cs_tr_to_o;
          LocalQueueElement * lqe = new LocalQueueElement(this, et_o_to_o, address);
          lqe->th_id = req_lqe->th_id;
          add_event_to_UL(curr_time, *(d_entry.sharedl2.begin()), lqe);
        }
        else
        {
          cout << "ctype = " << ctype << endl;
          display();  req_lqe->display();  geq->display();  ASSERTX(0);
        }
      }
      else if (etype == et_write)
      {
        switch (ctype)
        {
          case cs_exclusive:
            if (d_entry.sharedl2.size() != 1)
            {
              cout << "ctype = " << ctype << endl;
              display();  req_lqe->display();  geq->display();  ASSERTX(0);
            }

            if (d_entry.sharedl2.find(req_lqe->from.top()) != d_entry.sharedl2.end())
            {
              // move the state of the directory entry into the pending state
              num_e_to_tr++;
              d_entry.type  = cs_tr_to_m;
              req_lqe->type = et_write_nd;
              add_event_to_ULpp(curr_time, req_lqe, false);
            }
            else
            {
              // hold the request in the directory
              d_entry.pending = req_lqe;
              d_entry.got_cl  = false;
              // move the state of the directory entry into the pending state
              num_e_to_tr++;
              num_invalidate++;
              d_entry.type    = cs_tr_to_m;
              // generate requests to the L2s to move the state from exclusive to invalid
              LocalQueueElement * lqe = new LocalQueueElement();
              lqe->th_id = req_lqe->th_id;
              lqe->from.push(*(d_entry.sharedl2.begin()));
              lqe->from.push(this);
              lqe->type = et_invalidate;
              lqe->address = address;
              add_event_to_UL(curr_time, *(d_entry.sharedl2.begin()), lqe);
            }
            break;
            
          case cs_shared:
          case cs_owned:
            if (d_entry.sharedl2.empty() == true)
            {
              cout << "ctype = " << ctype << endl;
              display();  req_lqe->display();  geq->display();  ASSERTX(0);
            }
            // hold the request in the directory
            d_entry.pending = req_lqe;
            // move the state of the directory entry into the pending state
            if (ctype == cs_shared)
            {
              num_s_to_tr++;
            }
            else
            {
              num_o_to_tr++;
            }
            d_entry.type    = cs_tr_to_m;
            // generate requests to the L2s to move the state from modified to invalid

            if (use_limitless == true && limitless_broadcast_threshold < d_entry.sharedl2.size())
            {
              for (unsigned int l2idx = 0; l2idx < mcsim->l2s.size(); l2idx++)
              {
                num_invalidate++;
                LocalQueueElement * lqe = new LocalQueueElement();
                lqe->th_id = req_lqe->th_id;
                lqe->from.push(mcsim->l2s[l2idx]);
                lqe->from.push(this);
                lqe->type = (mcsim->l2s[l2idx] == *(d_entry.sharedl2.begin())) ? et_invalidate :
                            (d_entry.sharedl2.find(mcsim->l2s[l2idx]) != d_entry.sharedl2.end()) ? et_invalidate_nd :
                            et_nop;
                lqe->address = address;
                add_event_to_UL(curr_time, mcsim->l2s[l2idx], lqe);
              }
            }
            else
            {
              std::set<Component *>::iterator l2iter;
              for (l2iter = d_entry.sharedl2.begin(); l2iter != d_entry.sharedl2.end(); ++l2iter)
              {
                num_invalidate++;
                LocalQueueElement * lqe = new LocalQueueElement(*l2iter,
                  (l2iter == d_entry.sharedl2.begin()) ? et_invalidate : et_invalidate_nd,
                  address);
                lqe->th_id = req_lqe->th_id;
                lqe->from.push(this);
                add_event_to_UL(curr_time, *l2iter, lqe);
              }
            }
            break;

          case cs_modified:
            // invalidate L2 which has an old value
            if (d_entry.pending != NULL || d_entry.sharedl2.size() != 1)
            {
              cout << "ctype = " << ctype << endl;
              show_state(address);
              display();  req_lqe->display();  geq->display();  ASSERTX(0);
            }

            if (d_entry.sharedl2.find(req_lqe->from.top()) != d_entry.sharedl2.end())
            {
              // TODO -- currently miss after miss is treated as NACK.
              // We can do some optimization here since the L2 already has data.
              num_nack++;
              req_lqe->type = et_nack;
              // write miss does not update main memory
              add_event_to_UL(curr_time, req_lqe, false);
            }
            else
            {
              // hold the request in the directory
              d_entry.pending = req_lqe;
              // move the state of the directory entry into pending state
              num_m_to_tr++;
              num_invalidate++;
              d_entry.type = cs_tr_to_m;
              // generate a request to the L2 to move the state from modified to invalid
              LocalQueueElement * lqe = new LocalQueueElement(
                  *(d_entry.sharedl2.begin()), et_invalidate, address);
              lqe->th_id = req_lqe->th_id;
              lqe->from.push(this);
              add_event_to_UL(curr_time, *(d_entry.sharedl2.begin()), lqe);
            }
            break;

          case cs_invalid:
          default:
            cout << "ctype = " << ctype << endl;
            display();  req_lqe->display();  geq->display();  ASSERTX(0);
            break;
        }
      }
      else 
      {
        display();  req_lqe->display();  geq->display();  ASSERTX(0);
      } 
    }
  }
  else
  {
    geq->display(); 
    if (req_event_iter != req_event.end()) cout << req_event_iter->first << endl;
    if (rep_event_iter != rep_event.end()) cout << rep_event_iter->first << endl;
    ASSERTX(0);
  }

  return 0;
}

void Directory::remove_directory_cache_entry(uint32_t set, uint64_t dir_entry)
{
  if (has_directory_cache == true)
  {
    list<uint64_t> & curr_set = dir_cache[set];
    list<uint64_t>::iterator iter;

    for (iter = curr_set.begin(); iter != curr_set.end(); ++iter)
    {
      if (*iter == dir_entry)
      {
        curr_set.erase(iter);
        break;
      }
    }
  }
}


void Directory::add_event_to_UL(
    uint64_t curr_time,
    LocalQueueElement * lqe,
    bool is_data)
{
  if (lqe->from.top() == cachel2)
  {
    cachel2->add_rep_event(curr_time + dir_to_l2_t, lqe);
  }
  else
  {
    lqe->from.push(lqe->from.top());
    if (is_data == true)
    {
      crossbar->add_crq_event(curr_time+dir_to_xbar_t, lqe, num_flits_per_packet, this);
    }
    else
    {
      crossbar->add_crq_event(curr_time+dir_to_xbar_t, lqe, this);
    }
  }
} 


void Directory::add_event_to_UL(
    uint64_t curr_time,
    Component * comp,
    LocalQueueElement * lqe)
{
  if (comp == cachel2)
  {
    cachel2->add_rep_event(curr_time + dir_to_l2_t, lqe);
  }
  else
  {
    lqe->from.push(comp);
    crossbar->add_crq_event(curr_time+dir_to_xbar_t, lqe, this);
  }
} 


void Directory::add_event_to_ULpp(
    uint64_t curr_time,
    LocalQueueElement * lqe,
    bool is_data)
{
  if (lqe->from.top() == cachel2)
  {
    cachel2->add_rep_event(curr_time + dir_to_l2_t, lqe);
  }
  else
  {
    if (is_data == true)
    {
      crossbar->add_rep_event(curr_time+dir_to_xbar_t, lqe, num_flits_per_packet, this);
    }
    else
    {
      crossbar->add_rep_event(curr_time+dir_to_xbar_t, lqe, this);
    }
  }
}


