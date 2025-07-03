#ifndef NR_UE_BWP_MANAGER_H
#define NR_UE_BWP_MANAGER_H

#include "ns3/object.h"
#include <map>

namespace ns3 {

class NrUeBwpManager : public Object
{
public:
  static TypeId GetTypeId (void);
  NrUeBwpManager ();
  virtual ~NrUeBwpManager ();

  // BWP management
  void AddBwp (uint16_t bwpId, uint16_t numRbs);
  void RemoveBwp (uint16_t bwpId);
  uint16_t GetNumBwps () const;
  uint16_t GetNumRbs (uint16_t bwpId) const;
  uint16_t GetActiveUes (uint16_t bwpId) const;

  // UE management
  void AddUe (uint16_t ueId);
  void RemoveUe (uint16_t ueId);
  void SwitchBwp (uint16_t ueId, uint16_t newBwpId);
  uint16_t GetUeBwp (uint16_t ueId) const;
  const std::map<uint16_t, uint16_t>& GetUeMap () const;

protected:
  virtual void DoInitialize (void);
  virtual void DoDispose (void);

private:
  struct BwpInfo {
    uint16_t bwpId;
    uint16_t numRbs;
    uint16_t activeUes;
  };

  void NotifyPhyLayer (uint16_t ueId, uint16_t bwpId);

  std::map<uint16_t, BwpInfo> m_bwpMap; // BWP ID to BWP info
  std::map<uint16_t, uint16_t> m_ueMap; // UE ID to BWP ID
  uint16_t m_defaultBwpId;
  Time m_bwpSwitchLatency;
  uint64_t m_currentSlot;
};

} // namespace ns3

#endif /* NR_UE_BWP_MANAGER_H */
