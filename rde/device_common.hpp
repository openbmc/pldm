#pragma once

#include <libpldm/pldm_types.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace pldm::rde
{

/**
 * @enum DeviceState
 * @brief Represents the lifecycle state of an RDE-managed device.
 * It is owned and updated by the Device class, but may be influenced by
 * session outcomes.
 */
enum class DeviceState
{
    NotReady,    // Not initialized
    Discovering, // Actively probing device via RDE discovery
    Operational, // Device is ready and able to accept commands
    Busy,        // Device is executing a task or command
    Unreachable, // Communication failed (e.g., heartbeat timeout)
    Disabled     // Device manually or system-disabled
};

/**
 * @enum OpState
 * @brief Tracks the progress of discovery or operational commands.
 *
 * It is primarily used by DiscoverySession and OperationSession to manage
 * workflows.
 */
enum class OpState
{
    Idle,               // No operation triggered
    DiscoveryStarted,   // Initiated discovery sequence
    DiscoveryRunning,   // Discovery commands in progress
    DiscoveryCompleted, // Discovery finished successfully
    WaitingForResponse, // mc is waiting for response
    OperationQueued,    // Operational command is pending
    OperationExecuting, // Currently running operational command
    OperationCompleted, // Operational command finished successfully
    OperationFailed,    // Error during command execution
    Cancelled,          // Operation cancelled by host or timeout
    TimedOut            // Operation exceeded expected duration
};

/**
 * @enum DeviceCapabilitiesFlags
 * @brief Flags representing device capabilities.
 */
enum class DeviceCapabilitiesFlags : uint8_t
{
    None = 0,
    AtomicResourceRead = 1 << 0,
    ExpandSupport = 1 << 1,
    BejV1_1Support = 1 << 2
};

/**
 * @struct DeviceCapabilities
 * @brief Represents device capabilities using flags.
 */
struct DeviceCapabilities
{
    uint8_t value;

    /**
     * @brief Default constructor with pre-defined capability flags.
     */
    DeviceCapabilities() :
        value(
            static_cast<uint8_t>(DeviceCapabilitiesFlags::AtomicResourceRead) |
            static_cast<uint8_t>(DeviceCapabilitiesFlags::BejV1_1Support))
    {}

    /**
     * @brief Construct from raw uint8_t flags.
     */
    explicit DeviceCapabilities(uint8_t flags) : value(flags) {}

    /**
     * @brief Construct from bitfield8_t union.
     */
    explicit DeviceCapabilities(const bitfield8_t& bits) : value(bits.byte) {}

    /**
     * @brief Check if a specific capability flag is set.
     */
    constexpr bool has(DeviceCapabilitiesFlags flag) const
    {
        return (value & static_cast<uint8_t>(flag)) != 0;
    }

    /**
     * @brief Return raw capability value.
     */
    uint8_t get() const
    {
        return value;
    }

    /**
     * @brief Convert to bitfield8_t for interoperability.
     */
    bitfield8_t toBitfield() const
    {
        bitfield8_t bf{};
        bf.byte = value;
        return bf;
    }
};

/**
 * @enum FeatureBits
 * @brief Bit flags representing supported features.
 */
enum FeatureBits : uint16_t
{
    HeadSupported = 1 << 0,
    ReadSupported = 1 << 1,
    CreateSupported = 1 << 2,
    DeleteSupported = 1 << 3,
    UpdateSupported = 1 << 4,
    ReplaceSupported = 1 << 5,
    ActionSupported = 1 << 6,
    EventsSupported = 1 << 7,
    BejV1_1Supported = 1 << 8
};

/**
 * @struct FeatureSupport
 * @brief Represents a set of supported features.
 */
struct FeatureSupport
{
    uint16_t value = 0;

    FeatureSupport() = default;

    // Constructor from bitfield16_t
    explicit FeatureSupport(const bitfield16_t& bf) : value(bf.value) {}

    explicit FeatureSupport(uint16_t v) : value(v) {}

    constexpr bool has(FeatureBits bit) const
    {
        return (value & static_cast<uint16_t>(bit)) != 0;
    }
};

using MetadataVariant = std::variant<std::string, uint8_t, uint16_t, uint32_t,
                                     FeatureSupport, DeviceCapabilities>;

/**
 * @struct Metadata
 * @brief Metadata information for a device.
 */
struct Metadata
{
    FeatureSupport mcFeatureSupport = FeatureSupport(
        static_cast<uint16_t>(HeadSupported | ReadSupported | UpdateSupported));
    FeatureSupport devFeatureSupport{};
    uint8_t mcConcurrencySupport = 1;
    uint8_t deviceConcurrencySupport = 0;
    DeviceCapabilities devCapabilities{};
    uint32_t devConfigSignature = 0;
    std::string devProviderName;
    uint32_t mcMaxTransferChunkSizeBytes = 1024;
    uint32_t devMaxTransferChunkSizeBytes = 0;
    std::string etag;
    std::string protocolVersion = "1.0";
    std::string encoding = "application/json";
    std::vector<std::string> supportedOperations;
    uint32_t maxPayloadSize = 0;
    std::string sessionId;

    Metadata() = default;
};

} // namespace pldm::rde
