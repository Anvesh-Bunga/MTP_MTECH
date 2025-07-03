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

#include "nr-u-scheduler-ai.h"
#include "ns3/nr-u-bwp-manager.h"
#include "ns3/nr-u-lbt.h"
#include "ns3/nr-u-phy.h"
#include "ns3/nr-amc.h"
#include "ns3/nr-mac-scheduler.h"
#include "ns3/gym-bwp-rl-env.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NrUeAiScheduler");
NS_OBJECT_ENSURE_REGISTERED (NrUeAiScheduler);

TypeId
NrUeAiScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrUeAiScheduler")
    .SetParent<Object> ()
    .AddConstructor<NrUeAiScheduler> ()
    .AddAttribute ("AlgorithmType",
                   "Type of algorithm to use (LCA or RLA)",
                   EnumValue (RLA),
                   MakeEnumAccessor (&NrUeAiScheduler::m_algorithmType),
                   MakeEnumChecker (LCA, "LCA",
                                    RLA, "RLA"))
    .AddAttribute ("TimeWindowSize",
                   "Size of decision time window in slots",
                   UintegerValue (500),
                   MakeUintegerAccessor (&NrUeAiScheduler::m_timeWindowSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxScheduledUes",
                   "Maximum number of UEs that can be scheduled per slot",
                   UintegerValue (16),
                   MakeUintegerAccessor (&NrUeAiScheduler::m_maxScheduledUes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Epsilon",
                   "Initial exploration rate for RLA",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&NrUeAiScheduler::m_epsilon),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EpsilonMin",
                   "Minimum exploration rate for RLA",
                   DoubleValue (0.01),
                   MakeDoubleAccessor (&NrUeAiScheduler::m_epsilonMin),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EpsilonDecay",
                   "Epsilon decay rate for RLA",
                   DoubleValue (0.995),
                   MakeDoubleAccessor (&NrUeAiScheduler::m_epsilonDecay),
                   MakeDoubleChecker<double> ());
  return tid;
}

NrUeAiScheduler::NrUeAiScheduler ()
  : m_currentTimeSlot (0),
    m_currentWindow (0),
    m_algorithmType (RLA),
    m_rlEnv (nullptr)
{
  NS_LOG_FUNCTION (this);
}

NrUeAiScheduler::~NrUeAiScheduler ()
{
  NS_LOG_FUNCTION (this);
}

void
NrUeAiScheduler::SetBwpManager (Ptr<NrUeBwpManager> bwpManager)
{
  NS_LOG_FUNCTION (this << bwpManager);
  m_bwpManager = bwpManager;
}

void
NrUeAiScheduler::SetLbt (Ptr<NrUeLbt> lbt)
{
  NS_LOG_FUNCTION (this << lbt);
  m_lbt = lbt;
}

void
NrUeAiScheduler::SetPhy (Ptr<NrUePhy> phy)
{
  NS_LOG_FUNCTION (this << phy);
  m_phy = phy;
}

void
NrUeAiScheduler::SetGymEnv (Ptr<GymBwpRlEnv> rlEnv)
{
  NS_LOG_FUNCTION (this << rlEnv);
  m_rlEnv = rlEnv;
}

void
NrUeAiScheduler::DoInitialize ()
{
  NS_LOG_FUNCTION (this);
 
  // Initialize BWP statistics
  uint16_t numBwps = m_bwpManager->GetNumBwps ();
  for (uint16_t i = 0; i < numBwps; ++i)
  {
    BwpStats stats;
    stats.bwpId = i;
    stats.lbtFailureRate = 0.1 + (i * 0.2); // Initial values
    stats.wifiOccupancy = 0.2 + (i * 0.2); // Matches paper's 0.2,0.4,0.6
    stats.contentionWindow = 8;
    stats.avgBitsPerRb = 20.0;
    stats.totalThroughput = 0;
    stats.totalCollisions = 0;
    m_bwpStats.push_back (stats);
  }
 
  // Schedule first decision window
  Simulator::Schedule (MilliSeconds (0), &NrUeAiScheduler::RunDecisionWindow, this);
}

void
NrUeAiScheduler::RunDecisionWindow ()
{
  NS_LOG_FUNCTION (this);
 
  // Collect statistics over the window
  CollectWindowStatistics ();
 
  // Make BWP assignment decision
  if (m_algorithmType == LCA)
  {
    AssignBwpsLca ();
  }
  else
  {
    AssignBwpsRla ();
  }
 
  // Reset window statistics
  ResetWindowStatistics ();
 
  // Schedule next decision window
  m_currentWindow++;
  Simulator::Schedule (MilliSeconds (m_timeWindowSize * 0.5), // Assuming 0.5ms slots
                      &NrUeAiScheduler::RunDecisionWindow, this);
}

void
NrUeAiScheduler::CollectWindowStatistics ()
{
  NS_LOG_FUNCTION (this);
 
  // Collect LBT statistics from each BWP
  for (auto& stats : m_bwpStats)
  {
    stats.lbtFailureRate = m_lbt->GetFailureRate (stats.bwpId);
    stats.wifiOccupancy = m_lbt->GetWifiOccupancy (stats.bwpId);
    stats.contentionWindow = m_lbt->GetContentionWindow (stats.bwpId);
   
    // Update moving averages
    stats.avgBitsPerRb = 0.9 * stats.avgBitsPerRb +
                        0.1 * m_phy->GetAvgBitsPerRb (stats.bwpId);
  }
 
  // Collect UE statistics
  m_ueStats.clear ();
  auto ueMap = m_bwpManager->GetUeMap ();
  for (const auto& uePair : ueMap)
  {
    UeStats stats;
    stats.ueId = uePair.first;
    stats.currentBwp = uePair.second;
    stats.queueSize = m_phy->GetQueueSize (uePair.first);
    stats.holDelay = m_phy->GetHolDelay (uePair.first);
    stats.throughput = m_phy->GetThroughput (uePair.first);
    stats.avgBitsPerRb = m_phy->GetUeAvgBitsPerRb (uePair.first);
    m_ueStats.push_back (stats);
  }
}

void
NrUeAiScheduler::ResetWindowStatistics ()
{
  NS_LOG_FUNCTION (this);
 
  // Reset BWP throughput and collision counters
  for (auto& stats : m_bwpStats)
  {
    stats.totalThroughput = 0;
    stats.totalCollisions = 0;
  }
 
  // UE stats will be collected fresh each window
}

void
NrUeAiScheduler::AssignBwpsLca ()
{
  NS_LOG_FUNCTION (this);
 
  // Find BWP with maximum (1-F_n)*C_n*N_n^RB (Theorem 1)
  uint16_t bestBwp = 0;
  double maxMetric = 0.0;
 
  for (const auto& stats : m_bwpStats)
  {
    double metric = (1 - stats.lbtFailureRate) *
                   stats.avgBitsPerRb *
                   m_bwpManager->GetNumRbs (stats.bwpId);
   
    if (metric > maxMetric)
    {
      maxMetric = metric;
      bestBwp = stats.bwpId;
    }
  }
 
  // Assign all UEs to the best BWP (when |U| <= k)
  if (m_ueStats.size () <= m_maxScheduledUes)
  {
    for (const auto& ue : m_ueStats)
    {
      m_bwpManager->SwitchBwp (ue.ueId, bestBwp);
    }
  }
  else
  {
    // More complex assignment when |U| > k
    // Using proportional assignment based on BWP quality
    std::vector<double> bwpMetrics;
    for (const auto& stats : m_bwpStats)
    {
      bwpMetrics.push_back ((1 - stats.lbtFailureRate) *
                           stats.avgBitsPerRb *
                           m_bwpManager->GetNumRbs (stats.bwpId));
    }
   
    double totalMetric = std::accumulate (bwpMetrics.begin (), bwpMetrics.end (), 0.0);
    std::vector<uint16_t> uesPerBwp;
   
    for (size_t i = 0; i < bwpMetrics.size (); ++i)
    {
      uesPerBwp.push_back (std::round (m_ueStats.size () * (bwpMetrics[i] / totalMetric)));
    }
   
    // Distribute UEs
    uint16_t ueIdx = 0;
    for (size_t bwpIdx = 0; bwpIdx < m_bwpStats.size (); ++bwpIdx)
    {
      for (uint16_t i = 0; i < uesPerBwp[bwpIdx] && ueIdx < m_ueStats.size (); ++i, ++ueIdx)
      {
        m_bwpManager->SwitchBwp (m_ueStats[ueIdx].ueId, m_bwpStats[bwpIdx].bwpId);
      }
    }
  }
 
  NS_LOG_INFO ("LCA assigned UEs to BWP " << bestBwp << " with metric " << maxMetric);
}

void
NrUeAiScheduler::AssignBwpsRla ()
{
  NS_LOG_FUNCTION (this);
 
  if (!m_rlEnv)
  {
    NS_FATAL_ERROR ("RL environment not set for RLA algorithm");
    return;
  }
 
  // Get current state from environment
  Ptr<OpenGymSpace> currentState = m_rlEnv->GetObservationSpace ();
 
  // Get action from RL agent (epsilon-greedy)
  Ptr<OpenGymDataContainer> actionContainer;
  if ((rand () / (double)RAND_MAX) < m_epsilon)
  {
    // Random action
    actionContainer = m_rlEnv->GetActionSpace ()->Sample ();
    NS_LOG_INFO ("RLA taking random action (exploration)");
  }
  else
  {
    // Greedy action from RL model
    actionContainer = m_rlEnv->GetOptimalAction (currentState);
    NS_LOG_INFO ("RLA taking optimal action (exploitation)");
  }
 
  // Execute action (assign BWPs)
  std::vector<uint16_t> assignments = m_rlEnv->ExecuteAction (actionContainer);
 
  // Update epsilon
  m_epsilon = std::max (m_epsilon * m_epsilonDecay, m_epsilonMin);
 
  // Log assignments
  for (size_t i = 0; i < assignments.size (); ++i)
  {
    NS_LOG_DEBUG ("RLA assigned UE " << m_ueStats[i].ueId << " to BWP " << assignments[i]);
  }
}

void
NrUeAiScheduler::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_bwpManager = nullptr;
  m_lbt = nullptr;
  m_phy = nullptr;
  m_rlEnv = nullptr;
}

} // namespace ns3

