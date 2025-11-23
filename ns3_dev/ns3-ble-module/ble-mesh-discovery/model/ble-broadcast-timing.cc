/**
 * @file ble-broadcast-timing.cc
 * @brief NS-3 wrapper implementation for BLE broadcast timing
 */

#include "ble-broadcast-timing.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("BleBroadcastTiming");

NS_OBJECT_ENSURE_REGISTERED(BleBroadcastTiming);

TypeId
BleBroadcastTiming::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BleBroadcastTiming")
        .SetParent<Object>()
        .SetGroupName("BleMeshDiscovery")
        .AddConstructor<BleBroadcastTiming>();
    return tid;
}

BleBroadcastTiming::BleBroadcastTiming()
    : m_slotDuration(MilliSeconds(100)),
      m_rng(nullptr)
{
    NS_LOG_FUNCTION(this);
    
}

BleBroadcastTiming::~BleBroadcastTiming()
{
    NS_LOG_FUNCTION(this);
}

void
BleBroadcastTiming::Initialize(ble_broadcast_schedule_type_t scheduleType,
                                 uint32_t numSlots,
                                 Time slotDuration,
                                 double listenRatio)
{
    NS_LOG_FUNCTION(this << scheduleType << numSlots << slotDuration << listenRatio);

    m_slotDuration = slotDuration;

    
    ble_broadcast_timing_init(&m_state,
                               scheduleType,
                               numSlots,
                               slotDuration.GetMilliSeconds(),
                               listenRatio);

    

    NS_LOG_INFO("Initialized broadcast timing: "
                << "type=" << (scheduleType == BLE_BROADCAST_SCHEDULE_NOISY ? "NOISY" : "STOCHASTIC")
                << " slots=" << numSlots
                << " duration=" << slotDuration
                << " listenRatio=" << listenRatio);
}

void
BleBroadcastTiming::SetSeed(uint32_t seed)
{
    NS_LOG_FUNCTION(this << seed);
    ble_broadcast_timing_set_seed(&m_state, seed);
}

void
BleBroadcastTiming::SetRandomStream(Ptr<RandomVariableStream> stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rng = stream;
}

bool
BleBroadcastTiming::AdvanceSlot()
{
    NS_LOG_FUNCTION(this);

    
    if (m_rng)
    {
        double random_value = m_rng->GetValue();

        
        m_state.current_slot = (m_state.current_slot + 1) % m_state.num_slots;

        if (random_value < m_state.listen_ratio)
        {
            m_state.is_broadcast_slot = false;
            m_state.total_listen_slots++;
        }
        else
        {
            m_state.is_broadcast_slot = true;
            m_state.total_broadcast_slots++;
            m_state.broadcast_attempts++;
        }

        NS_LOG_DEBUG("Slot " << m_state.current_slot
                     << " (random=" << random_value << "): "
                     << (m_state.is_broadcast_slot ? "BROADCAST" : "LISTEN"));

        return m_state.is_broadcast_slot;
    }
    else
    {
        
        bool isBroadcast = ble_broadcast_timing_advance_slot(&m_state);

        NS_LOG_DEBUG("Slot " << m_state.current_slot << ": "
                     << (isBroadcast ? "BROADCAST" : "LISTEN"));

        return isBroadcast;
    }
}

bool
BleBroadcastTiming::ShouldBroadcast() const
{
    return ble_broadcast_timing_should_broadcast(&m_state);
}

bool
BleBroadcastTiming::ShouldListen() const
{
    return ble_broadcast_timing_should_listen(&m_state);
}

void
BleBroadcastTiming::RecordSuccess()
{
    NS_LOG_FUNCTION(this);
    ble_broadcast_timing_record_success(&m_state);

    NS_LOG_INFO("Broadcast success recorded. Total successes: "
                << m_state.successful_broadcasts
                << " Success rate: " << GetSuccessRate());
}

bool
BleBroadcastTiming::RecordFailure()
{
    NS_LOG_FUNCTION(this);
    bool shouldRetry = ble_broadcast_timing_record_failure(&m_state);

    NS_LOG_INFO("Broadcast failure recorded. Retry count: "
                << m_state.retry_count
                << "/" << m_state.max_retries
                << " Should retry: " << (shouldRetry ? "yes" : "no"));

    return shouldRetry;
}

void
BleBroadcastTiming::ResetRetry()
{
    NS_LOG_FUNCTION(this);
    ble_broadcast_timing_reset_retry(&m_state);
}

double
BleBroadcastTiming::GetSuccessRate() const
{
    return ble_broadcast_timing_get_success_rate(&m_state);
}

uint32_t
BleBroadcastTiming::GetCurrentSlot() const
{
    return ble_broadcast_timing_get_current_slot(&m_state);
}

double
BleBroadcastTiming::GetActualListenRatio() const
{
    return ble_broadcast_timing_get_actual_listen_ratio(&m_state);
}

Time
BleBroadcastTiming::GetSlotDuration() const
{
    return m_slotDuration;
}

uint32_t
BleBroadcastTiming::GetNumSlots() const
{
    return m_state.num_slots;
}

} 