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

#include "bwp-rl-env.h"
#include "ns3/nr-u-scheduler-ai.h"
#include "ns3/nr-u-bwp-manager.h"
#include "ns3/nr-u-lbt.h"
#include "ns3/nr-u-phy.h"
#include "ns3/log.h"
#include "ns3/opengym_interface.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("GymBwpRlEnv");
NS_OBJECT_ENSURE_REGISTERED (GymBwpRlEnv);

TypeId
GymBwpRlEnv::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GymBwpRlEnv")
    .SetParent<OpenGymEnv> ()
    .AddConstructor<GymBwpRlEnv> ()
    .AddAttribute ("Alpha",
                   "Weight for delay in reward calculation",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&GymBwpRlEnv::m_alpha),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Beta",
                   "Weight for throughput in reward calculation",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&GymBwpRlEnv::m_beta),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxThroughput",
                   "Maximum achievable throughput for normalization",
                   DoubleValue (1000.0),
                   MakeDoubleAccessor (&GymBwpRlEnv::m_maxThroughput),
                   MakeDoubleChecker<double> ());
  return tid;
}

GymBwpRlEnv::GymBwpRlEnv ()
  : m_scheduler (nullptr),
    m_currentStep (0),
    m_episode (0),
    m_totalReward (0.0)
{
  NS_LOG_FUNCTION (this);
}

GymBwpRlEnv::~GymBwpRlEnv ()
{
  NS_LOG_FUNCTION (this);
}

void
GymBwpRlEnv::SetScheduler (Ptr<NrUeAiScheduler> scheduler)
{
  NS_LOG_FUNCTION (this << scheduler);
  m_scheduler = scheduler;
}

void
GymBwpRlEnv::DoInitialize ()
{
  NS_LOG_FUNCTION (this);
 
  // Initialize observation space
  m_observationSpace = CreateObject<OpenGymBoxSpace> (GetObservationSpaceShape ());
 
  // Initialize action space (discrete BWP assignments)
  m_actionSpace = CreateObject<OpenGymDiscreteSpace> (m_scheduler->GetNumBwps ());
 
  OpenGymEnv::DoInitialize ();
}

Ptr<OpenGymSpace>
GymBwpRlEnv::GetObservationSpace (void)
{
  NS_LOG_FUNCTION (this);
  return m_observationSpace;
}

Ptr<OpenGymSpace>
GymBwpRlEnv::GetActionSpace (void)
{
  NS_LOG_FUNCTION (this);
  return m_actionSpace;
}

std::vector<uint32_t>
GymBwpRlEnv::GetObservationSpaceShape (void) const
{
  NS_LOG_FUNCTION (this);
 
  // State dimensions based on paper's equation (16) and (17):
  // UE states: [L, B, C, D, P] + one-hot BWP encoding
  // BWP states: [M, F, CW] per BWP
  uint32_t numUes = m_scheduler->GetNumUes ();
  uint32_t numBwps = m_scheduler->GetNumBwps ();
  uint32_t ueStateSize = 5 + numBwps; // 5 metrics + one-hot encoding
  uint32_t bwpStateSize = 3; // M, F, CW
 
  return {1, numUes * ueStateSize + numBwps * bwpStateSize};
}

bool
GymBwpRlEnv::GetGameOver (void)
{
  NS_LOG_FUNCTION (this);
 
  // End episode after fixed number of steps
  // (Could also use convergence criteria)
  return m_currentStep >= 1000; // 1000 steps per episode
}

Ptr<OpenGymDataContainer>
GymBwpRlEnv::GetObservation (void)
{
  NS_LOG_FUNCTION (this);
 
  // Get current state from scheduler
  auto ueStats = m_scheduler->GetUeStats ();
  auto bwpStats = m_scheduler->GetBwpStats ();
 
  uint32_t numUes = ueStats.size ();
  uint32_t numBwps = bwpStats.size ();
 
  // Create container for observation
  Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>> (GetObservationSpaceShape ());
 
  // Fill UE states (equation 16)
  for (const auto& ue : ueStats)
  {
    // Basic UE metrics
    box->AddValue (ue.queueSize);       // L
    box->AddValue (ue.holDelay);        // B
    box->AddValue (ue.avgBitsPerRb);    // C
    box->AddValue (ue.throughput);      // D (using throughput as proxy for P)
    box->AddValue (ue.avgBitsPerRb);    // P (reusing same value)
   
    // One-hot BWP encoding
    for (uint32_t i = 0; i < numBwps; ++i)
    {
      box->AddValue (ue.currentBwp == i ? 1.0 : 0.0);
    }
  }
 
  // Fill BWP states (set J_n in equation 17)
  for (const auto& bwp : bwpStats)
  {
    box->AddValue (bwp.wifiOccupancy);      // M
    box->AddValue (bwp.lbtFailureRate);     // F
    box->AddValue (bwp.contentionWindow);   // CW
  }
 
  return box;
}

float
GymBwpRlEnv::GetReward (void)
{
  NS_LOG_FUNCTION (this);
 
  // Calculate reward based on equation (2) from paper
  double avgHolDelay = 0.0;
  double totalThroughput = 0.0;
 
  auto ueStats = m_scheduler->GetUeStats ();
  for (const auto& ue : ueStats)
  {
    avgHolDelay += ue.holDelay;
    totalThroughput += ue.throughput;
  }
 
  avgHolDelay /= ueStats.size ();
 
  // R[t_w] = -(α*avgHolDelay + β*(T_max - totalThroughput))
  float reward = -(m_alpha * avgHolDelay + m_beta * (m_maxThroughput - totalThroughput));
 
  m_totalReward += reward;
  return reward;
}

std::string
GymBwpRlEnv::GetExtraInfo (void)
{
  NS_LOG_FUNCTION (this);
 
  std::stringstream ss;
  ss << "{\"episode\": " << m_episode
     << ", \"step\": " << m_currentStep
     << ", \"total_reward\": " << m_totalReward << "}";
 
  return ss.str ();
}

bool
GymBwpRlEnv::ExecuteActions (Ptr<OpenGymDataContainer> action)
{
  NS_LOG_FUNCTION (this);
 
  // Convert action to BWP assignments
  Ptr<OpenGymDiscreteContainer> discrete = DynamicCast<OpenGymDiscreteContainer> (action);
  if (!discrete)
  {
    NS_LOG_ERROR ("Invalid action type");
    return false;
  }
 
  uint32_t numUes = m_scheduler->GetNumUes ();
  uint32_t numBwps = m_scheduler->GetNumBwps ();
 
  // Simple action: Assign all UEs to same BWP (for initial implementation)
  // Could extend to per-UE assignments with more complex action space
  uint16_t selectedBwp = discrete->GetValue () % numBwps;
 
  auto ueStats = m_scheduler->GetUeStats ();
  for (const auto& ue : ueStats)
  {
    m_scheduler->SwitchBwp (ue.ueId, selectedBwp);
  }
 
  NS_LOG_INFO ("Executed action - assigned all UEs to BWP " << selectedBwp);
  return true;
}

Ptr<OpenGymDataContainer>
GymBwpRlEnv::GetOptimalAction (Ptr<OpenGymSpace> state)
{
  NS_LOG_FUNCTION (this);
 
  // For initial implementation, use same heuristic as LCA
  // In full implementation, this would use the trained DRQN model
 
  auto bwpStats = m_scheduler->GetBwpStats ();
  uint16_t bestBwp = 0;
  double maxMetric = 0.0;
 
  for (const auto& stats : bwpStats)
  {
    double metric = (1 - stats.lbtFailureRate) *
                   stats.avgBitsPerRb *
                   m_scheduler->GetNumRbs (stats.bwpId);
   
    if (metric > maxMetric)
    {
      maxMetric = metric;
      bestBwp = stats.bwpId;
    }
  }
 
  Ptr<OpenGymDiscreteContainer> action = CreateObject<OpenGymDiscreteContainer> (m_actionSpace);
  action->SetValue (bestBwp);
 
  return action;
}

void
GymBwpRlEnv::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_scheduler = nullptr;
  OpenGymEnv::DoDispose ();
}

} // namespace ns3

