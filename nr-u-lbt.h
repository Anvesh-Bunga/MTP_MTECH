#ifndef NR_UE_LBT_H
#define NR_UE_LBT_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include <map>

namespace ns3 {

class NrUePhy;

/**
 * \brief Implements Listen Before Talk (LBT) functionality for NR-U
 *
 * This class handles the channel access procedure in unlicensed spectrum
 * according to 3GPP specifications, including both ICCA and ECCA phases.
 */
class NrUeLbt : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NrUeLbt ();
  virtual ~NrUeLbt ();

  /**
   * \brief Set the PHY layer
   * \param phy The PHY layer
   */
  void SetPhy (Ptr<NrUePhy> phy);

  /**
   * \brief Add a BWP to manage LBT for
   * \param bwpId The BWP identifier
   * \param wifiPoissonMean Mean interval for WiFi interference
   */
  void AddBwp (uint16_t bwpId, double wifiPoissonMean);

  /**
   * \brief Request channel access
   * \param bwpId The BWP identifier
   * \return true if access granted, false otherwise
   */
  bool ChannelAccessRequest (uint16_t bwpId);

  // Statistics getters
  double GetFailureRate (uint16_t bwpId) const;
  double GetWifiOccupancy (uint16_t bwpId) const;
  uint16_t GetContentionWindow (uint16_t bwpId) const;

  /**
   * \brief Configure WiFi interference parameters
   * \param bwpId The BWP identifier
   * \param poissonMean New Poisson mean for WiFi interference
   */
  void SetWifiInterference (uint16_t bwpId, double poissonMean);

protected:
  virtual void DoDispose (void);

private:
  /// BWP-specific LBT state
  struct BwpLbtState {
    uint16_t bwpId;                 ///< BWP identifier
    uint16_t currentCw;             ///< Current contention window size
    double wifiPoissonMean;         ///< WiFi interference rate
    double wifiOccupancy;           ///< Measured WiFi occupancy
    double lbtFailureRate;          ///< LBT failure rate
    uint32_t totalAttempts;         ///< Total access attempts
    uint32_t totalFailures;         ///< Total access failures
    Time channelBusyUntil;          ///< Time until channel is busy
    Time channelOccupiedUntil;      ///< Time until we occupy channel
    Time lastUpdateTime;            ///< Last statistics update time
  };

  void ScheduleWifiInterference (uint16_t bwpId);
  void HandleWifiInterference (uint16_t bwpId);
  void UpdateFailureRate (uint16_t bwpId);

  Ptr<NrUePhy> m_phy;                          ///< PHY layer
  Ptr<UniformRandomVariable> m_uniformRandom;   ///< Random number generator
  std::map<uint16_t, BwpLbtState> m_bwpStates; ///< Per-BWP LBT state

  // Parameters
  uint16_t m_cwMin;        ///< Minimum contention window
  uint16_t m_cwMax;        ///< Maximum contention window
  uint16_t m_iccaDuration; ///< ICCA duration in slots
  uint16_t m_mcotDuration; ///< Maximum Channel Occupancy Time
};

} // namespace ns3

#endif /* NR_UE_LBT_H */
