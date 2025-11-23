#include "ble-discovery-cycle-wrapper.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryCycleWrapper");
NS_OBJECT_ENSURE_REGISTERED (BleDiscoveryCycleWrapper);

TypeId
BleDiscoveryCycleWrapper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleDiscoveryCycleWrapper")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleDiscoveryCycleWrapper> ()
  ;
  return tid;
}

BleDiscoveryCycleWrapper::BleDiscoveryCycleWrapper ()
{
  NS_LOG_FUNCTION (this);
  ble_discovery_cycle_init (&m_cycle);
}

BleDiscoveryCycleWrapper::~BleDiscoveryCycleWrapper ()
{
  NS_LOG_FUNCTION (this);
  Stop ();
}

void
BleDiscoveryCycleWrapper::Start ()
{
  NS_LOG_FUNCTION (this);

  if (!ble_discovery_cycle_start (&m_cycle))
    {
      NS_LOG_WARN ("Discovery cycle already running");
      return;
    }

  NS_LOG_INFO ("Starting discovery cycle with slot duration: "
               << m_cycle.slot_duration_ms << " ms");

  ScheduleAllSlots ();
}

void
BleDiscoveryCycleWrapper::Stop ()
{
  NS_LOG_FUNCTION (this);

  if (!ble_discovery_cycle_is_running (&m_cycle))
    {
      return;
    }

  ble_discovery_cycle_stop (&m_cycle);
  CancelAllEvents ();

  NS_LOG_INFO ("Discovery cycle stopped");
}

bool
BleDiscoveryCycleWrapper::IsRunning () const
{
  return ble_discovery_cycle_is_running (&m_cycle);
}

void
BleDiscoveryCycleWrapper::SetSlotDuration (Time duration)
{
  NS_LOG_FUNCTION (this << duration);

  uint32_t duration_ms = static_cast<uint32_t> (duration.GetMilliSeconds ());

  if (!ble_discovery_cycle_set_slot_duration (&m_cycle, duration_ms))
    {
      NS_LOG_WARN ("Cannot change slot duration while cycle is running");
    }
}

Time
BleDiscoveryCycleWrapper::GetSlotDuration () const
{
  return MilliSeconds (ble_discovery_cycle_get_slot_duration (&m_cycle));
}

Time
BleDiscoveryCycleWrapper::GetCycleDuration () const
{
  return MilliSeconds (ble_discovery_cycle_get_cycle_duration (&m_cycle));
}

uint8_t
BleDiscoveryCycleWrapper::GetCurrentSlot () const
{
  return ble_discovery_cycle_get_current_slot (&m_cycle);
}

uint32_t
BleDiscoveryCycleWrapper::GetCycleCount () const
{
  return ble_discovery_cycle_get_cycle_count (&m_cycle);
}

void
BleDiscoveryCycleWrapper::SetSlot0Callback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_slot0Callback = callback;
}

void
BleDiscoveryCycleWrapper::SetForwardingSlotCallback (uint8_t slotNumber, Callback<void> callback)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (slotNumber));

  if (!ble_discovery_cycle_is_forwarding_slot (slotNumber))
    {
      NS_LOG_ERROR ("Invalid forwarding slot number: " << static_cast<uint32_t> (slotNumber));
      return;
    }

  switch (slotNumber)
    {
    case 1:
      m_slot1Callback = callback;
      break;
    case 2:
      m_slot2Callback = callback;
      break;
    case 3:
      m_slot3Callback = callback;
      break;
    }
}

void
BleDiscoveryCycleWrapper::SetCycleCompleteCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_cycleCompleteCallback = callback;
}

void
BleDiscoveryCycleWrapper::ExecuteSlot0 ()
{
  NS_LOG_FUNCTION (this);

  
  m_cycle.current_slot = 0;

  NS_LOG_DEBUG ("Executing Slot 0 - Own message transmission");

  if (!m_slot0Callback.IsNull ())
    {
      m_slot0Callback ();
    }
}

void
BleDiscoveryCycleWrapper::ExecuteForwardingSlot (uint8_t slotNumber)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (slotNumber));

  
  m_cycle.current_slot = slotNumber;

  NS_LOG_DEBUG ("Executing Slot " << static_cast<uint32_t> (slotNumber) << " - Forwarding");

  Callback<void> callback;

  switch (slotNumber)
    {
    case 1:
      callback = m_slot1Callback;
      break;
    case 2:
      callback = m_slot2Callback;
      break;
    case 3:
      callback = m_slot3Callback;
      break;
    default:
      NS_LOG_ERROR ("Invalid slot number");
      return;
    }

  if (!callback.IsNull ())
    {
      callback ();
    }
}

void
BleDiscoveryCycleWrapper::ScheduleNextCycle ()
{
  NS_LOG_FUNCTION (this);

  if (!ble_discovery_cycle_is_running (&m_cycle))
    {
      return;
    }

  
  m_cycle.cycle_count++;

  
  if (!m_cycleCompleteCallback.IsNull ())
    {
      m_cycleCompleteCallback ();
    }

  NS_LOG_DEBUG ("Cycle " << m_cycle.cycle_count << " complete, scheduling next cycle");

  
  m_cycle.current_slot = 0;

  
  ScheduleAllSlots ();
}

void
BleDiscoveryCycleWrapper::ScheduleAllSlots ()
{
  Time slotDuration = GetSlotDuration ();

  
  m_slot0Event = Simulator::Schedule (Seconds (0),
                                       &BleDiscoveryCycleWrapper::ExecuteSlot0,
                                       this);

  
  m_slot1Event = Simulator::Schedule (slotDuration,
                                       &BleDiscoveryCycleWrapper::ExecuteForwardingSlot,
                                       this, (uint8_t) 1);
  m_slot2Event = Simulator::Schedule (slotDuration * 2,
                                       &BleDiscoveryCycleWrapper::ExecuteForwardingSlot,
                                       this, (uint8_t) 2);
  m_slot3Event = Simulator::Schedule (slotDuration * 3,
                                       &BleDiscoveryCycleWrapper::ExecuteForwardingSlot,
                                       this, (uint8_t) 3);

  
  m_cycleEvent = Simulator::Schedule (slotDuration * 4,
                                       &BleDiscoveryCycleWrapper::ScheduleNextCycle,
                                       this);
}

void
BleDiscoveryCycleWrapper::CancelAllEvents ()
{
  Simulator::Cancel (m_slot0Event);
  Simulator::Cancel (m_slot1Event);
  Simulator::Cancel (m_slot2Event);
  Simulator::Cancel (m_slot3Event);
  Simulator::Cancel (m_cycleEvent);
}

} 
