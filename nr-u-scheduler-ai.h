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

#ifndef NR_UE_AI_SCHEDULER_H
#define NR_UE_AI_SCHEDULER_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include <vector>
#include <map>

namespace ns3 {

class NrUeBwpManager;
class NrUeLbt;
class NrUePhy;
class GymBwpRlEnv;

/**
 * \brief AI-based scheduler for NR-U Bandwidth Part assignment
 *
 * This class implements two algorithms for BWP assignment:
 * 1. Least Collision Assignment (LCA) - A heuristic-based approach
 * 2. Reinforcement Learning Assignment (RLA) - DRQN-based approach
 */
class NrUeAiScheduler : public Object
{
public:
  /**
   * \brief Algorithm types
   */
  enum AlgorithmType {
    LCA,  ///< Least Collision Assignment
    RLA   ///< Reinforcement Learning Assignment
  };

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NrUeAiScheduler ();
  virtual ~NrUeAiScheduler ();

  /**
   * \brief Set the BWP manager
   * \param bwpManager The BWP manager
   */
  void SetBwpManager (Ptr<NrUeBwpManager> bwpManager);

  /**
   * \brief Set the LBT component
   * \param lbt The LBT component
   */
  void SetLbt (Ptr<NrUeLbt> lbt);

  /**
   * \brief Set the PHY layer
   * \param phy The PHY layer
   */
  void SetPhy (Ptr<NrUePhy> phy);

  /**
   * \brief Set the RL environment
   * \param rlEnv The RL environment
   */
  void SetGymEnv (Ptr<GymBwpRlEnv> rlEnv);

protected:
  virtual void DoInitialize (void);
  virtual void DoDispose (void);

private:
  /**
   * \brief BWP statistics structure
   */
  struct BwpStats {
    uint16_t bwpId;             ///< BWP identifier
    double lbtFailureRate;      ///< LBT failure rate
    double wifiOccupancy;       ///< WiFi channel occupancy
    double contentionWindow;    ///< Current contention window size
    double avgBitsPerRb;        ///< Average bits per resource block
    double totalThroughput;     ///< Total throughput in this window
    uint32_t totalCollisions;   ///< Total collisions in this window
  };

  /**
   * \brief UE statistics structure
   */
  struct UeStats {
    uint16_t ueId;              ///< UE identifier
    uint16_t currentBwp;        ///< Current BWP assignment
    uint32_t queueSize;         ///< Current queue size
    double holDelay;            ///< Head-of-Line delay
    double throughput;          ///< Throughput in this window
    double avgBitsPerRb;        ///< UE-specific bits per RB
  };

  // Core methods
  void RunDecisionWindow (void);
  void CollectWindowStatistics (void);
  void ResetWindowStatistics (void);
  void AssignBwpsLca (void);
  void AssignBwpsRla (void);

  // Member variables
  Ptr<NrUeBwpManager> m_bwpManager; ///< BWP manager
  Ptr<NrUeLbt> m_lbt;               ///< LBT component
  Ptr<NrUePhy> m_phy;               ///< PHY layer
  Ptr<GymBwpRlEnv> m_rlEnv;         ///< RL environment

  uint32_t m_currentTimeSlot;       ///< Current time slot
  uint32_t m_currentWindow;         ///< Current decision window
  AlgorithmType m_algorithmType;    ///< Selected algorithm type
  uint32_t m_timeWindowSize;        ///< Decision window size in slots
  uint32_t m_maxScheduledUes;       ///< Max UEs schedulable per slot

  // RL parameters
  double m_epsilon;                 ///< Exploration rate
  double m_epsilonMin;              ///< Minimum exploration rate
  double m_epsilonDecay;            ///< Exploration rate decay

  std::vector<BwpStats> m_bwpStats; ///< BWP statistics
  std::vector<UeStats> m_ueStats;   ///< UE statistics
};

} // namespace ns3

#endif /* NR_UE_AI_SCHEDULER_H */

