/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 Samsung R&D Institute India - Bangalore.
 * Author: Ram Aditya S <ram.aditya@samsung.com>
 *         Shyamal Dhua <shyamal.dhua@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "nr-u-lbt.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"
#include "ns3/nr-u-phy.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NrUeLbt");
NS_OBJECT_ENSURE_REGISTERED (NrUeLbt);

TypeId
NrUeLbt::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrUeLbt")
    .SetParent<Object> ()
    .AddConstructor<NrUeLbt> ()
    .AddAttribute ("CwMin",
                   "Minimum contention window size",
                   UintegerValue (8),
                   MakeUintegerAccessor (&NrUeLbt::m_cwMin),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("CwMax",
                   "Maximum contention window size",
                   UintegerValue (128),
                   MakeUintegerAccessor (&NrUeLbt::m_cwMax),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("IccaDuration",
                   "ICCA duration in slots",
                   UintegerValue (1),
                   MakeUintegerAccessor (&NrUeLbt::m_iccaDuration),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("McotDuration",
                   "Maximum Channel Occupancy Time in slots",
                   UintegerValue (5),
                   MakeUintegerAccessor (&NrUeLbt::m_mcotDuration),
                   MakeUintegerChecker<uint16_t> ());
  return tid;
}

NrUeLbt::NrUeLbt ()
  : m_phy (nullptr)
{
  NS_LOG_FUNCTION (this);
  m_uniformRandom = CreateObject<UniformRandomVariable> ();
}

NrUeLbt::~NrUeLbt ()
{
  NS_LOG_FUNCTION (this);
}

void
NrUeLbt::SetPhy (Ptr<NrUePhy> phy)
{
  NS_LOG_FUNCTION (this << phy);
  m_phy = phy;
}

void
NrUeLbt::AddBwp (uint16_t bwpId, double wifiPoissonMean)
{
  NS_LOG_FUNCTION (this << bwpId << wifiPoissonMean);
 
  BwpLbtState state;
  state.bwpId = bwpId;
  state.currentCw = m_cwMin;
  state.wifiPoissonMean = wifiPoissonMean;
  state.wifiOccupancy = 0.0;
  state.lbtFailureRate = 0.0;
  state.totalAttempts = 0;
  state.totalFailures = 0;
  state.lastUpdateTime = Simulator::Now ();
 
  m_bwpStates[bwpId] = state;
 
  // Schedule WiFi interference events
  ScheduleWifiInterference (bwpId);
}

void
NrUeLbt::ScheduleWifiInterference (uint16_t bwpId)
{
  NS_LOG_FUNCTION (this << bwpId);
 
  auto& state = m_bwpStates[bwpId];
  Time interval = Seconds (1.0) / state.wifiPoissonMean;
 
  Simulator::Schedule (interval, &NrUeLbt::HandleWifiInterference, this, bwpId);
}

void
NrUeLbt::HandleWifiInterference (uint16_t bwpId)
{
  NS_LOG_FUNCTION (this << bwpId);
 
  auto& state = m_bwpStates[bwpId];
 
  // Mark channel as busy for random duration (1-5 slots)
  uint16_t busySlots = 1 + (rand () % 5);
  state.channelBusyUntil = Simulator::Now () + MilliSeconds (busySlots * 0.5); // 0.5ms slots
 
  // Update WiFi occupancy statistics
  Time now = Simulator::Now ();
  Time delta = now - state.lastUpdateTime;
  state.wifiOccupancy = (state.wifiOccupancy * 0.9) + (0.1 * (busySlots * 0.5 / delta.GetSeconds ()));
  state.lastUpdateTime = now;
 
  // Schedule next interference event
  ScheduleWifiInterference (bwpId);
}

bool
NrUeLbt::ChannelAccessRequest (uint16_t bwpId)
{
  NS_LOG_FUNCTION (this << bwpId);
 
  auto& state = m_bwpStates[bwpId];
  state.totalAttempts++;
 
  // ICCA - Immediate check
  if (Simulator::Now () < state.channelBusyUntil)
  {
    NS_LOG_DEBUG ("ICCA failed for BWP " << bwpId);
    state.totalFailures++;
    UpdateFailureRate (bwpId);
    return false;
  }
 
  // ECCA - Backoff procedure
  uint16_t backoffSlots = m_uniformRandom->GetInteger (0, state.currentCw - 1);
  Time backoffTime = MilliSeconds (backoffSlots * 0.5); // 0.5ms slots
 
  NS_LOG_DEBUG ("ECCA backoff for BWP " << bwpId << ": " << backoffSlots << " slots");
 
  Time endTime = Simulator::Now () + backoffTime;
 
  // Check for interruptions during backoff
  if (endTime > state.channelBusyUntil)
  {
    NS_LOG_DEBUG ("ECCA interrupted by WiFi for BWP " << bwpId);
    state.totalFailures++;
    UpdateFailureRate (bwpId);
   
    // Double CW for next attempt (up to max)
    state.currentCw = std::min (2 * state.currentCw, m_cwMax);
    return false;
  }
 
  // Success - reset CW and grant channel access
  state.currentCw = m_cwMin;
  state.channelOccupiedUntil = Simulator::Now () + MilliSeconds (m_mcotDuration);
 
  NS_LOG_DEBUG ("Channel access granted for BWP " << bwpId << " for " << m_mcotDuration << " slots");
  return true;
}

void
NrUeLbt::UpdateFailureRate (uint16_t bwpId)
{
  NS_LOG_FUNCTION (this << bwpId);
 
  auto& state = m_bwpStates[bwpId];
 
  // Exponential moving average of failure rate
  double currentRate = (double)state.totalFailures / state.totalAttempts;
  state.lbtFailureRate = (0.9 * state.lbtFailureRate) + (0.1 * currentRate);
 
  NS_LOG_DEBUG ("Updated LBT failure rate for BWP " << bwpId << ": " << state.lbtFailureRate);
}

double
NrUeLbt::GetFailureRate (uint16_t bwpId) const
{
  NS_LOG_FUNCTION (this << bwpId);
  auto it = m_bwpStates.find (bwpId);
  if (it != m_bwpStates.end ())
  {
    return it->second.lbtFailureRate;
  }
  return 0.0;
}

double
NrUeLbt::GetWifiOccupancy (uint16_t bwpId) const
{
  NS_LOG_FUNCTION (this << bwpId);
  auto it = m_bwpStates.find (bwpId);
  if (it != m_bwpStates.end ())
  {
    return it->second.wifiOccupancy;
  }
  return 0.0;
}

uint16_t
NrUeLbt::GetContentionWindow (uint16_t bwpId) const
{
  NS_LOG_FUNCTION (this << bwpId);
  auto it = m_bwpStates.find (bwpId);
  if (it != m_bwpStates.end ())
  {
    return it->second.currentCw;
  }
  return m_cwMin;
}

void
NrUeLbt::SetWifiInterference (uint16_t bwpId, double poissonMean)
{
  NS_LOG_FUNCTION (this << bwpId << poissonMean);
  auto it = m_bwpStates.find (bwpId);
  if (it != m_bwpStates.end ())
  {
    it->second.wifiPoissonMean = poissonMean;
  }
}

void
NrUeLbt::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_phy = nullptr;
  m_bwpStates.clear ();
}

} // namespace ns3

