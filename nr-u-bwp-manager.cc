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

#include "nr-u-bwp-manager.h"
#include "ns3/log.h"
#include "ns3/nr-phy.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NrUeBwpManager");
NS_OBJECT_ENSURE_REGISTERED (NrUeBwpManager);

TypeId
NrUeBwpManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrUeBwpManager")
    .SetParent<Object> ()
    .AddConstructor<NrUeBwpManager> ()
    .AddAttribute ("DefaultBwpId",
                   "Default BWP ID for initial UE attachment",
                   UintegerValue (0),
                   MakeUintegerAccessor (&NrUeBwpManager::m_defaultBwpId),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("BwpSwitchLatency",
                   "Time required for BWP switching",
                   TimeValue (MilliSeconds (1)),
                   MakeTimeAccessor (&NrUeBwpManager::m_bwpSwitchLatency),
                   MakeTimeChecker ());
  return tid;
}

NrUeBwpManager::NrUeBwpManager ()
  : m_currentSlot (0)
{
  NS_LOG_FUNCTION (this);
}

NrUeBwpManager::~NrUeBwpManager ()
{
  NS_LOG_FUNCTION (this);
}

void
NrUeBwpManager::DoInitialize ()
{
  NS_LOG_FUNCTION (this);
}

void
NrUeBwpManager::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_ueMap.clear ();
  m_bwpMap.clear ();
}

void
NrUeBwpManager::AddBwp (uint16_t bwpId, uint16_t numRbs)
{
  NS_LOG_FUNCTION (this << bwpId << numRbs);
 
  BwpInfo bwpInfo;
  bwpInfo.bwpId = bwpId;
  bwpInfo.numRbs = numRbs;
  bwpInfo.activeUes = 0;
 
  m_bwpMap[bwpId] = bwpInfo;
  NS_LOG_INFO ("Added BWP " << bwpId << " with " << numRbs << " RBs");
}

void
NrUeBwpManager::RemoveBwp (uint16_t bwpId)
{
  NS_LOG_FUNCTION (this << bwpId);
 
  auto it = m_bwpMap.find (bwpId);
  if (it != m_bwpMap.end ())
  {
    // Reassign UEs from this BWP to default BWP
    for (auto& uePair : m_ueMap)
    {
      if (uePair.second == bwpId)
      {
        uePair.second = m_defaultBwpId;
        m_bwpMap[m_defaultBwpId].activeUes++;
      }
    }
   
    m_bwpMap.erase (it);
    NS_LOG_INFO ("Removed BWP " << bwpId);
  }
}

uint16_t
NrUeBwpManager::GetNumBwps () const
{
  return m_bwpMap.size ();
}

uint16_t
NrUeBwpManager::GetNumRbs (uint16_t bwpId) const
{
  auto it = m_bwpMap.find (bwpId);
  if (it != m_bwpMap.end ())
  {
    return it->second.numRbs;
  }
  return 0;
}

uint16_t
NrUeBwpManager::GetActiveUes (uint16_t bwpId) const
{
  auto it = m_bwpMap.find (bwpId);
  if (it != m_bwpMap.end ())
  {
    return it->second.activeUes;
  }
  return 0;
}

void
NrUeBwpManager::AddUe (uint16_t ueId)
{
  NS_LOG_FUNCTION (this << ueId);
 
  if (m_ueMap.find (ueId) == m_ueMap.end ())
  {
    m_ueMap[ueId] = m_defaultBwpId;
    m_bwpMap[m_defaultBwpId].activeUes++;
    NS_LOG_INFO ("Added UE " << ueId << " to default BWP " << m_defaultBwpId);
  }
}

void
NrUeBwpManager::RemoveUe (uint16_t ueId)
{
  NS_LOG_FUNCTION (this << ueId);
 
  auto it = m_ueMap.find (ueId);
  if (it != m_ueMap.end ())
  {
    uint16_t bwpId = it->second;
    m_bwpMap[bwpId].activeUes--;
    m_ueMap.erase (it);
    NS_LOG_INFO ("Removed UE " << ueId << " from BWP " << bwpId);
  }
}

void
NrUeBwpManager::SwitchBwp (uint16_t ueId, uint16_t newBwpId)
{
  NS_LOG_FUNCTION (this << ueId << newBwpId);
 
  auto ueIt = m_ueMap.find (ueId);
  auto bwpIt = m_bwpMap.find (newBwpId);
 
  if (ueIt != m_ueMap.end () && bwpIt != m_bwpMap.end ())
  {
    uint16_t oldBwpId = ueIt->second;
   
    if (oldBwpId != newBwpId)
    {
      // Update counts
      m_bwpMap[oldBwpId].activeUes--;
      m_bwpMap[newBwpId].activeUes++;
     
      // Update UE mapping
      ueIt->second = newBwpId;
     
      NS_LOG_INFO ("Switched UE " << ueId << " from BWP " << oldBwpId
                   << " to BWP " << newBwpId);
     
      // Notify PHY about BWP switch with configured latency
      Simulator::Schedule (m_bwpSwitchLatency, &NotifyPhyLayer, ueId, newBwpId);
    }
  }
  else
  {
    NS_LOG_WARN ("Invalid UE " << ueId << " or BWP " << newBwpId << " for switching");
  }
}

uint16_t
NrUeBwpManager::GetUeBwp (uint16_t ueId) const
{
  auto it = m_ueMap.find (ueId);
  if (it != m_ueMap.end ())
  {
    return it->second;
  }
  return m_defaultBwpId; // Fallback to default
}

const std::map<uint16_t, uint16_t>&
NrUeBwpManager::GetUeMap () const
{
  return m_ueMap;
}

void
NrUeBwpManager::NotifyPhyLayer (uint16_t ueId, uint16_t bwpId)
{
  NS_LOG_FUNCTION (ueId << bwpId);
  // In actual implementation, this would notify the PHY layer
  // about the BWP switch for the specified UE
}

} // namespace ns3

