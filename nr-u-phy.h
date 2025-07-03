//#include "nr-u-phy.h"
#include "ns3/log.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NrUPhy");
NS_OBJECT_ENSURE_REGISTERED (NrUPhy);

TypeId
NrUPhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NrUPhy")
    .SetParent<NrPhy> ()
    .AddConstructor<NrUPhy> ()
    .AddAttribute ("TxPower",
                  "Transmission power in dBm",
                  DoubleValue (30.0),
                  MakeDoubleAccessor (&NrUPhy::m_txPower),
                  MakeDoubleChecker<double> ());
  return tid;
}

NrUPhy::NrUPhy () : m_txPower (30.0)
{
  NS_LOG_FUNCTION (this);
}

void
NrUPhy::ConfigureBwp (uint16_t bwpId, uint16_t numerology, double scs, uint16_t rbs)
{
  NS_LOG_FUNCTION (this << bwpId << numerology << scs << rbs);
  
  if (bwpId >= m_bwpConfigs.size ())
  {
    m_bwpConfigs.resize (bwpId + 1);
  }

  m_bwpConfigs[bwpId] = {
    .numerology = numerology,
    .subcarrierSpacing = scs,
    .numRbs = rbs,
    .txPower = NrSpectrumValueHelper::CreateTxPowerSpectralDensity (m_txPower, rbs, scs)
  };
}

void
NrUPhy::UpdateChannelQuality (uint16_t rnti, const std::vector<double>& cqi)
{
  NS_LOG_FUNCTION (this << rnti);
  m_cqiMap[rnti] = cqi;
}

std::vector<uint16_t>
NrUPhy::AllocateResources (uint16_t bwpId, const std::vector<uint16_t>& ues)
{
  NS_LOG_FUNCTION (this << bwpId);
  std::vector<uint16_t> allocatedRbs;

  if (bwpId >= m_bwpConfigs.size () || m_bwpConfigs[bwpId].numRbs == 0)
  {
    NS_LOG_WARN ("Invalid BWP ID or no RBs configured");
    return allocatedRbs;
  }

  // Simple round-robin allocation (replace with PF scheduler in real implementation)
  uint16_t rbPerUe = m_bwpConfigs[bwpId].numRbs / std::max(1, (int)ues.size());
  for (uint16_t ue : ues)
  {
    if (m_cqiMap.find(ue) != m_cqiMap.end())
    {
      // Allocate best RBs based on CQI (simplified)
      for (uint16_t rb = 0; rb < rbPerUe; ++rb)
      {
        allocatedRbs.push_back(rb);
      }
    }
  }

  return allocatedRbs;
}

void
NrUPhy::DoInitialize ()
{
  NS_LOG_FUNCTION (this);
  NrPhy::DoInitialize ();
}

void
NrUPhy::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_bwpConfigs.clear ();
  m_cqiMap.clear ();
  NrPhy::DoDispose ();
}

} // namespace ns3
