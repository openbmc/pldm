# PLDM - Platform Level Data Model

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

## Overview

PLDM (Platform Level Data Model) is a key component of the OpenBMC project,
providing a standardized data model and message formats for various platform
management functionalities. It defines a method to manage, monitor, and control
the firmware and hardware of a system.

The OpenBMC PLDM project aims to implement the specifications defined by the
Distributed Management Task Force (DMTF), allowing for interoperable management
interfaces across different hardware and firmware components.

## Features

- **Standardized Messaging:** Adheres to the DMTF's PLDM specifications,
  enabling consistent and interoperable communication between different
  components.
- **Modularity:** Supports multiple PLDM types, including base, FRU,Firmware
  update, Platform Monitoring and Control, and BIOS Control and Configuration.
- **Extensibility:** Easily extendable to support new PLDM types and custom OEM
  commands.
- **Integration:** Seamlessly integrates with other OpenBMC components for
  comprehensive system management.

## Getting Started

### Prerequisites

To build and run PLDM, you need the following dependencies:

- `Meson`
- `Ninja`

Alternatively, source an OpenBMC ARM/x86 SDK.

### Building

To build the PLDM project, follow these steps:

```bash
meson setup build && meson compile -C build
```

### To run unit tests

The simplest way of running the tests is as described by the meson man page:

```bash
meson test -C build
```

Alternatively, tests can be run in the OpenBMC CI docker container using
[these](https://github.com/openbmc/docs/blob/master/testing/local-ci-build.md)
steps.

### To enable pldm verbosity

pldm daemon accepts a command line argument `--verbose` or `--v` or `-v` to
enable the daemon to run in verbose mode. It can be done via adding this option
to the environment file that pldm service consumes.

```bash
echo 'PLDMD_ARGS="--verbose"' > /etc/default/pldmd
systemctl restart pldmd
```

### To disable pldm verbosity

```bash
rm /etc/default/pldmd
systemctl restart pldmd
```

## Documentation

For complete documentation on the functionality and usage of this repository,
please refer to the [docs](docs) folder.
