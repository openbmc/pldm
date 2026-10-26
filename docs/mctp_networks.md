# MCTP Networks

This document describes MCTP Network support in pldmd. It explains why an
endpoint must be identified by its Network and EID rather than by its EID alone,
how pldmd does that, and how to enable it. The support is optional and off by
default; without it pldmd addresses endpoints by EID, as before.

## Problem

An MCTP EID is only unique within one MCTP Network ([DSP0236 v1.3.3][1], lines
736-738). With the af-mctp transport a system can have several Networks, so the
same EID can name two different endpoints. The MCTP D-Bus interface already
reports the Network of every endpoint, and `MctpInfo` carries it, but nothing
below the discovery uses it. pldmd identifies a remote endpoint by its EID
everywhere:

- the transport maps every valid EID to the TID of the same value when it is
  created, on any Network [2]
- the requester, the instance ID database and the message queues are keyed by
  that value [3]
- fw-update keys the discovered firmware devices by it and looks up, therefore
  Entity Manager configuration by EID alone [4]
- platform-mc maps a PLDM TID to an `MctpInfo` and sends to the EID in it [5]

Two endpoints that have the same EID on different Networks therefore cannot be
told apart. Requests to either go to the "any Network" mapping of that EID,
which the kernel resolves to the default Network, so the endpoint on the other
Network is never reached. It is still discovered, and it shares its state with
the first one: both use the same descriptor entry, and removing one removes it
for the other.

The log messages have the same gap. They say "endpoint ID 9", which does not say
on which Network. For firmware update, which runs for every device and can take
long, an operator cannot tell which device a message is about.

## Goals

- Support more than one MCTP Network with the af-mctp transport: an endpoint is
  identified by its Network and EID, so the same EID can be used on two
  Networks, and each endpoint is reached, discovered and updated on its own.
- Tell endpoints apart by Network and EID in everything below the discovery,
  instead of by EID alone.
- Make the log messages of firmware update, and of the code its requests go
  through, name the Network and the EID of the device, the Network first.
- Do not change the behaviour of existing setups: with one Network, or with the
  mctp-demux transport, pldmd must behave as it does today.

## Design

### Three identities

pldmd keeps three things apart that used to be one number:

| Name       | What it is                                 | Owned by           |
| ---------- | ------------------------------------------ | ------------------ |
| `Endpoint` | Network and EID, as the MCTP daemon has it | D-Bus, discovery   |
| `PeerId`   | Key for transport, requester, instance IDs | `EndpointRegistry` |
| PLDM TID   | Terminus ID, assigned by SetTID            | platform-mc        |

The `PeerId` is what the requester stack calls the EID today. It is the libpldm
TID that the transport maps to an endpoint. With one Network it is the EID, so
nothing changes for it.

### Endpoint registry

An `EndpointRegistry` is the one place that knows both the `Endpoint` and the
`PeerId` of an endpoint. Discovery asks it for the `PeerId` of every endpoint it
finds and releases it when the endpoint is removed. How a `PeerId` is chosen is
a policy:

- The `identity policy` always uses the EID as the `PeerId`. It is simple and
  keeps today's behaviour, but two endpoints with the same EID on different
  Networks cannot both be served: the first one found keeps the `PeerId` and the
  other is skipped. This is the default.
- The `pooled policy` uses the EID as the `PeerId` whenever that value is free,
  so with a single Network it gives exactly the same result as the identity
  policy. Only when the EID is already taken by an endpoint on another Network
  does it hand out a different free `PeerId` instead, so that both endpoints can
  be used.

The registry also tells the transport which Network and EID a `PeerId` stands
for, when it assigns the `PeerId` and when it gives it back, so the two cannot
get out of step.

### Handlers get the PeerId

`MctpDiscoveryHandlerIntf` gets `handleEndpoints()` and
`handleRemovedEndpoints()`, which pass the `PeerId` with each endpoint. By
default they call the existing `handleMctpEndpoints()` and
`handleRemovedMctpEndpoints()`, so a handler moves to the new ones when it is
ready and no handler has to change at once. fw-update moves to the new methods.
platform-mc keeps the old ones, and its terminus manager gets the `PeerId` of an
endpoint from the registry. They do not depend on each other.

### Transport

`PldmTransport` gets `mapEndpoint()` and `unmapEndpoint()`, which bind a
`PeerId` to a Network and EID with libpldm's
`pldm_transport_af_mctp_map_tid_fqe()`. With the build option below, the
transport starts with no endpoint known, and only the endpoints that discovery
binds can be reached.

### Build option

A build option `network-support`, disabled by default and only valid with the
af-mctp transport, turns it on, like `transport-implementation` selects the
transport. Without it the transport keeps the mappings it creates itself and the
`identity policy` is used, so a single Network behaves as before. The
`identity policy` does not give an endpoint a second `PeerId` for an EID that is
already in use on another Network, nor one for a reserved EID, so pldmd now logs
and skips such an endpoint instead of giving it to the handlers, where it would
have been addressed as the other endpoint. With this option pldmd uses the
`pooled policy`, binds each endpoint when it is discovered, and the transport
only knows discovered endpoints.

### Devices reachable more than once

A device can be reachable over more than one Network or under more than one EID.
It then shows up as one endpoint for each, and would get several termini and
firmware inventory entries. With `network-support`, endpoints that share a
non-empty UUID are one device. The UUID is what identifies a device.

- The endpoint discovered first is given to fw-update and platform-mc. The
  others stand by: they get no `PeerId` and are not bound in the transport.
- An endpoint is only given to them once it answers a PLDM `GetTID`, as one that
  is on D-Bus can be dead, for example the old registration of a device that
  came back with another EID. One that does not answer goes back to standby and
  the next one is tried, until one answers or none is left. One that did not
  answer is tried again when it becomes available again. Only the first request
  is checked: an endpoint that stops answering later does not switch the device,
  as below.
- When the endpoint that is in use is removed, an available endpoint that stands
  by takes its place. An endpoint that is not available keeps standing by, and
  takes over when it becomes available while none is in use. The device is
  removed from fw-update and platform-mc and added again on the new endpoint,
  with the `PeerId` of that endpoint, so that the terminus, the sensors and the
  firmware inventory are discovered again on it. They have to be: what a device
  offers can differ from one interface to the other.
- Losing connectivity to the endpoint in use does not switch the device to a
  standby endpoint. The endpoint is only marked as unavailable, exactly as it is
  without grouping, and the device keeps its terminus, sensors and firmware
  inventory until connectivity returns. A switch removes the device and adds it
  again, so a short loss of connectivity would discard all of that and discover
  it again, and would abort a firmware update that may be in progress.
- An endpoint with an empty UUID is never grouped.

### Logs

An endpoint is named `Network <m> EID <n>` in the log messages, resolved from
the `PeerId` through the registry. When the registry does not know the `PeerId`,
it is logged as it is.

## Compatibility and risk

- The existing handler interface is kept, and every step of the change keeps the
  behaviour of the default build, so the steps can be merged one by one.
- Code that is not migrated, such as host-bmc and the OEM code, still takes the
  `PeerId` it is given to be the EID, for example in its log messages. That is
  right for every endpoint whose `PeerId` is its EID: all of them with a single
  Network, and the first one found when an EID is shared between Networks. It is
  wrong for an endpoint that had to get another `PeerId` because its EID was
  already taken, which that code then shows under the other number.
- A `PeerId` has 8 bits, so at most 247 endpoints can be addressed.

## Turning it on

Build pldmd with the af-mctp transport and the `network-support` option:

```bash
meson setup build -Dtransport-implementation=af-mctp -Dnetwork-support=enabled
```

The option is off by default. With mctp-demux, which has no Networks,
configuring it is an error.

## Limitations

- Messages from an endpoint that was not discovered are not accepted. This is on
  purpose. An endpoint has to show up on the MCTP D-Bus interface first. For the
  same reason a request cannot be sent to one: the requests of the host-bmc and
  OEM code to the host EID need the host to be an endpoint on that interface.
- Code outside fw-update and platform-mc, for example the host-bmc and OEM code,
  still reads the `PeerId` as an EID and logs the EID only. That is right as
  long as the endpoint is the only one with its EID, which the `pooled policy`
  favours, but not for an endpoint that had to be given another `PeerId`.
- There are at most 247 `PeerId`s, and an endpoint that cannot get one is
  skipped.
- Which endpoint of a device is "discovered first" is only arrival order for
  endpoints that appear while pldmd runs. The endpoints that are already there
  at start-up are taken in the order of their EID, then UUID, then Network, so
  the lowest EID becomes the one in use.
- An endpoint keeps its `PeerId` for as long as it is on the MCTP D-Bus
  interface, including while it is unavailable. The `PeerId` is given back only
  when the endpoint is removed.
- Only one endpoint of a device is used at a time, and the device only switches
  to another one when the one in use is removed. There is no priority between
  the endpoints, and a device is not moved to a better endpoint that appears
  while the one in use is there.
- `pldmtool` allocates instance IDs with the EID as the key, and pldmd with the
  `PeerId`. This is only a problem when all of these hold:
  - `network-support` is enabled,
  - the endpoint had to be given a `PeerId` other than its EID, because another
    Network already uses that EID, and
  - `pldmtool` is used on that endpoint while pldmd is talking to it.

  Then both can use the same instance ID for the same device at the same time.
  In every other case, including every setup with a single Network, the two keys
  are the same.

## pldmtool

`pldmtool` talks to one endpoint at a time. Give its Network with `-N` or
`--network` when it is not on the default Network, for example

```bash
pldmtool base GetTID -m 9 -N 2
```

This needs the af-mctp transport.

## References

[1]:
  https://www.dmtf.org/sites/default/files/standards/documents/DSP0236_1.3.3.pdf
[2]:
  https://github.com/openbmc/pldm/blob/dfb79e88936418708941ee030078dc032be20a53/common/transport.cpp#L65
[3]:
  https://github.com/openbmc/pldm/blob/dfb79e88936418708941ee030078dc032be20a53/requester/request.hpp#L192
[4]:
  https://github.com/openbmc/pldm/blob/dfb79e88936418708941ee030078dc032be20a53/fw-update/firmware_inventory_manager.cpp#L106
[5]:
  https://github.com/openbmc/pldm/blob/dfb79e88936418708941ee030078dc032be20a53/platform-mc/terminus_manager.cpp#L447
