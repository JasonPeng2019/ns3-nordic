/**
 * @file ble-broadcast-timing.h
 * @brief NS-3 wrapper for BLE broadcast timing (Tasks 12 & 14)
 *
 * C++ wrapper that delegates to pure C core implementation.
 */

#ifndef BLE_BROADCAST_TIMING_WRAPPER_H
#define BLE_BROADCAST_TIMING_WRAPPER_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ble_broadcast_timing.h"

namespace ns3 {

/**
 * @brief NS-3 wrapper for BLE broadcast timing
 *
 * Thin C++ wrapper that delegates all logic to C core.
 * Handles NS-3 specific concerns (Time, RandomVariableStream, TypeId).
 */
class BleBroadcastTiming : public Object
{
public:
    /**
     * @brief Get TypeId for NS-3 object system
     * @return TypeId
     */
    static TypeId GetTypeId();

    /**
     * @brief Constructor
     */
    BleBroadcastTiming();

    /**
     * @brief Destructor
     */
    virtual ~BleBroadcastTiming();

    /**
     * @brief Initialize broadcast timing
     * @param scheduleType Schedule type (NOISY or STOCHASTIC)
     * @param numSlots Number of time slots
     * @param slotDuration Duration of each slot
     * @param listenRatio Probability of listening (0.0-1.0)
     */
    void Initialize(ble_broadcast_schedule_type_t scheduleType,
                    uint32_t numSlots,
                    Time slotDuration,
                    double listenRatio);

    /**
     * @brief Set random seed
     * @param seed Random seed value
     */
    void SetSeed(uint32_t seed);

    /**
     * @brief Set random number stream
     * @param stream Random variable stream
     */
    void SetRandomStream(Ptr<RandomVariableStream> stream);

    /**
     * @brief Advance to next slot
     * @return true if current slot is for broadcasting
     */
    bool AdvanceSlot();

    /**
     * @brief Check if should broadcast in current slot
     * @return true if should broadcast
     */
    bool ShouldBroadcast() const;

    /**
     * @brief Check if should listen in current slot
     * @return true if should listen
     */
    bool ShouldListen() const;

    /**
     * @brief Record successful broadcast
     */
    void RecordSuccess();

    /**
     * @brief Record failed broadcast
     * @return true if should retry
     */
    bool RecordFailure();

    /**
     * @brief Reset retry counter
     */
    void ResetRetry();

    /**
     * @brief Adjust schedule based on crowding factor
     * @param crowdingFactor Crowding value (0.0-1.0)
     */
    void SetCrowdingFactor(double crowdingFactor);

    /**
     * @brief Get broadcast success rate
     * @return Success rate (0.0-1.0)
     */
    double GetSuccessRate() const;

    /**
     * @brief Get current slot index
     * @return Current slot index
     */
    uint32_t GetCurrentSlot() const;

    /**
     * @brief Get actual listen ratio observed
     * @return Actual listen ratio (0.0-1.0)
     */
    double GetActualListenRatio() const;

    /**
     * @brief Get slot duration
     * @return Slot duration
     */
    Time GetSlotDuration() const;

    /**
     * @brief Get number of slots
     * @return Number of slots
     */
    uint32_t GetNumSlots() const;

private:
    ble_broadcast_timing_t m_state;  /**< C core state */
    Time m_slotDuration;              /**< NS-3 Time for slot duration */
    Ptr<RandomVariableStream> m_rng;  /**< NS-3 random number generator */
};

} // namespace ns3

#endif /* BLE_BROADCAST_TIMING_WRAPPER_H */
