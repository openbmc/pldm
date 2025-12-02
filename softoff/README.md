# PLDM Soft Power Off

## Overview

The `pldm-softpoweroff` application provides graceful host shutdown functionality using the Platform Level Data Model (PLDM) protocol. It coordinates with the host system to perform a controlled shutdown by sending PLDM Set State Effecter States commands and monitoring the shutdown completion through state sensor events.

## Purpose

This application is responsible for:
- Initiating graceful shutdown or restart requests to remote PLDM terminus (typically the host)
- Monitoring the host's current state before initiating shutdown
- Waiting for shutdown completion confirmation via PLDM state sensor events
- Handling timeout scenarios if the host fails to respond or complete shutdown

## Architecture

### Components

#### Main Application (`main.cpp`)
- Entry point for the pldm-softpoweroff service
- Initializes the event loop and D-Bus connection
- Creates and manages the `SoftPowerOff` instance
- Handles error conditions and exit codes

#### SoftPowerOff Class (`softoff.hpp`, `softoff.cpp`)
- Core implementation of the soft power off logic
- Manages PLDM communication with the host
- Handles state monitoring and timeout management
- Coordinates with systemd through D-Bus signals

### Key Features

1. **Host State Verification**: Checks if the host is in a valid state (Running or TransitioningToOff) before initiating shutdown
2. **PDR-based Discovery**: Dynamically discovers the appropriate effecter and sensor IDs from Platform Descriptor Records (PDRs)
3. **Graceful Shutdown/Restart**: Supports both shutdown and restart operations based on the requested host transition
4. **Event-driven Completion**: Monitors PLDM StateSensorEvent signals to detect shutdown completion
5. **Timeout Protection**: Implements configurable timeout (default 30 seconds) to prevent indefinite waiting

## Configuration

### Configuration File

Location: `configurations/softoff/softoff_config.json`

The configuration file defines the PLDM terminus and entity information for soft power off:

```json
{
    "entries": [
        {
            "tid": 208,
            "entityType": 32801,
            "stateSetId": 129
        },
        {
            "tid": 2,
            "entityType": 45,
            "stateSetId": 129
        }
    ]
}
```

**Parameters:**
- `tid`: PLDM Terminus ID of the remote endpoint
- `entityType`: PLDM entity type hosting the soft power off effecter
- `stateSetId`: State set ID for the software termination status (typically 129 for `PLDM_STATE_SET_SW_TERMINATION_STATUS`)

The application tries each entry in order until it finds matching PDRs.

### Environment Variables

- `SOFTOFF_TIMEOUT_SECONDS`: Timeout duration for host shutdown completion (default: implementation-defined)
- `SOFTOFF_CONFIG_JSON`: Path to the configuration directory (default: set at build time)

## Operation Flow

1. **Initialization**
   - Parse configuration file
   - Verify host is in Running or TransitioningToOff state
   - Query PDR repository to find matching effecter and sensor IDs

2. **Shutdown Request**
   - Determine operation type (shutdown vs. restart) from D-Bus property
   - Encode PLDM Set State Effecter States request
   - Send request to the host via PLDM transport
   - Start response timeout timer (30 seconds)

3. **Response Handling**
   - Wait for PLDM response message
   - Validate response completion code
   - Start shutdown completion timer

4. **Completion Monitoring**
   - Subscribe to D-Bus StateSensorEvent signals
   - Wait for graceful shutdown event from host
   - Stop timer upon successful completion

5. **Timeout/Error Handling**
   - Report timeout errors via D-Bus
   - Exit with appropriate error codes
   - Clean up instance IDs

## D-Bus Interface

### Consumed Interfaces

- **xyz.openbmc_project.State.Host**
  - Properties:
    - `CurrentHostState`: Current state of the host
    - `RequestedHostTransition`: Requested transition (Off or Reboot)

- **xyz.openbmc_project.PLDM.PDR**
  - Methods:
    - `FindStateEffecterPDR`: Locate effecter PDR by entity type and state set
    - `FindStateSensorPDR`: Locate sensor PDR by entity type and state set

- **xyz.openbmc_project.PLDM.Event**
  - Signals:
    - `StateSensorEvent`: Notification of state sensor changes

### Error Reporting

Errors are reported via phosphor-logging:
- `xyz.openbmc_project.PLDM.Error.SoftPowerOff.HostSoftOffTimeOut`: Timeout waiting for host shutdown

## Systemd Integration

### Service Unit

File: `services/pldmSoftPowerOff.service`

The service is configured as:
- **Type**: oneshot (runs once and exits)
- **Dependencies**: 
  - Wants: `pldmd.service` (PLDM daemon must be available)
  - After: `pldmd.service` (starts after PLDM daemon)
  - Before: `obmc-host-stop-pre@0.target` (runs before host stop sequence)
  - Conflicts: `obmc-host-startmin@0.target` (prevents running during host start)

The service is typically triggered by the host shutdown sequence in OpenBMC.

## Building

The application is built using Meson:

```bash
meson setup build
ninja -C build
```

### Dependencies

- libpldm: PLDM protocol library
- sdbusplus: D-Bus C++ bindings
- sdeventplus: Event loop wrapper
- phosphor-logging: Logging framework
- phosphor-dbus-interfaces: D-Bus interface definitions
- nlohmann-json: JSON parsing library

## Exit Codes

- `0`: Success (shutdown completed or host already in non-running state)
- `-1`: Error conditions:
  - Failed to get host state
  - Failed to find required PDRs
  - Failed to send PLDM request
  - Timeout waiting for response or completion
  - Invalid response from host

## Debugging

### Log Messages

The application uses structured logging (lg2) with the following key messages:

- Configuration parsing errors
- Host state verification results
- PDR lookup success/failure
- PLDM message send/receive status
- Timer events and timeout conditions
- Shutdown completion events

### Common Issues

1. **Configuration file not found**: Verify `SOFTOFF_CONFIG_JSON` path and file existence
2. **PDR not found**: Ensure host has published appropriate effecter and sensor PDRs
3. **Timeout errors**: Check host responsiveness and PLDM communication
4. **D-Bus errors**: Verify pldmd service is running and D-Bus permissions are correct

## Related Components

- **pldmd**: Main PLDM daemon that manages PDR repository and event handling
- **Host firmware**: Must support PLDM soft power off effecter and sensor
- **obmc-host-shutdown**: OpenBMC host shutdown orchestration

## References

- DMTF DSP0248: Platform Level Data Model (PLDM) for Platform Monitoring and Control
- DMTF DSP0249: PLDM State Set Specifications
- State Set 129: Software Termination Status

