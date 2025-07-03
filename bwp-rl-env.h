#ifndef GYM_BWP_RL_ENV_H
#define GYM_BWP_RL_ENV_H

#include "ns3/opengym-module.h"


//#include "ns3/opengym-discrete-space.h"
#include "ns3/opengym_env.h"  // If it's in the opengym module

#include "ns3/nr-u-phy.h"


// MODIFIED: Removed non-existent headers and kept only the available ones
#include "ns3/opengym_interface.h"  // Contains OpenGymEnv base class
#include "ns3/spaces.h"             // Contains BoxSpace and DiscreteSpace
#include "ns3/nr-u-scheduler-ai.h"  // For NrUeAiScheduler

namespace ns3 {

class GymBwpRlEnv : public OpenGymEnv
{
public:
  static TypeId GetTypeId (void);
  GymBwpRlEnv ();
  virtual ~GymBwpRlEnv ();

  void SetScheduler (Ptr<NrUeAiScheduler> scheduler);

  // OpenGymEnv interface implementation
  virtual Ptr<OpenGymSpace> GetObservationSpace (void);
  virtual Ptr<OpenGymSpace> GetActionSpace (void);
  virtual bool GetGameOver (void);
  virtual Ptr<OpenGymDataContainer> GetObservation (void);
  virtual float GetReward (void);
  virtual std::string GetExtraInfo (void);
  virtual bool ExecuteActions (Ptr<OpenGymDataContainer> action);

  // Helper methods
  Ptr<OpenGymDataContainer> GetOptimalAction (Ptr<OpenGymSpace> state);

protected:
  virtual void DoInitialize (void);
  virtual void DoDispose (void);

private:
  std::vector<uint32_t> GetObservationSpaceShape (void) const;

  Ptr<NrUeAiScheduler> m_scheduler;
  
  // MODIFIED: Changed from OpenGymBoxSpace to BoxSpace
  Ptr<BoxSpace> m_observationSpace;
  
  // MODIFIED: Changed from OpenGymDiscreteSpace to DiscreteSpace
  Ptr<DiscreteSpace> m_actionSpace;

  uint32_t m_currentStep;
  uint32_t m_episode;
  float m_totalReward;

  // Reward parameters
  double m_alpha;
  double m_beta;
  double m_maxThroughput;
};

} // namespace ns3

#endif /* GYM_BWP_RL_ENV_H */
