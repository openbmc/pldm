## Overview of pldmtool

pldmtool is a client tool that acts as a PLDM requester which runs on the BMC.
pldmtool accepts the request message and display the response message & provides
flexibility to parse the response message and display in the read-able format.

pldmtool supports the subcommands for all PLDM types such as base, platform,
bios, fru, and oem-ibm.

- Libraries are implemented in c++
- Consumes libpldm encode and decode functions
- Communicates with pldmd daemon


## pldmtool repo

Library files in pldmtool repo are named with respective to PLDM type.

Example:

pldm_base_cmd.[hpp/cpp], pldm_fru_cmd.[hpp/cpp]

pldmtool commands for corresponding PLDM type is constructed with help of
encode request and decode response APIs which are implemented in libpldm
along with handlers implemented in libpldmresponder.


Example:

Given a PLDM command "foo" of PLDM type "base" the pldmtool library should
consume following API from the libpldm/base.h

- encode_foo_req() - To send the required input parameters in the request message,
- decode_foo_resp() - Decode the response message and display in JSON format.

If PLDM commands are not yet constructed in the pldmtool repo user can directly
send the request message with the help of 'pldmtool raw -d <data>' option


## Usuage

User can see the pldmtool supported PLDM types in the usage output
available with the -h help option as shown below:

```
pldmtool -h
PLDM requester tool for OpenBMC
Usage: pldmtool [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
  raw                         send a raw request and print response
  base                        base type command
  bios                        bios type command
  platform                    platform type command
  fru                         FRU type command
  oem-ibm                     oem type command

```
pldmtool command prompt expects a PLDM type to see the list of supported 
commands that are already implemented for that particular PLDM type.

Command format : pldmtool <pldmType> -h
Example:         pldmtool base -h

```
$ pldmtool base -h

base type command
Usage: pldmtool base [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
  GetPLDMTypes                get pldm supported types
  GetPLDMVersion              get version of a certain type
  GetTID                      get Terminus ID (TID)
  GetPLDMCommands             get supported commands of pldm type

```
More help on the command usage can be found by specifying the PLDM type and the
command name with -h argument as shown below

Command format : pldmtool <pldmType> <commandName> -h
Example:         pldmtool base GetPLDMTypes -h

```
$ pldmtool base GetPLDMTypes -h

get pldm supported types
Usage: pldmtool base GetPLDMTypes [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -m,--mctp_eid UINT          MCTP endpoint ID
  -v,--verbose
```


## pldmtool raw command usage

pldmtool raw command option accepts request message in the hexa decimal
bytes and send the response message in hexa decimal bytes.

```
$ pldmtool raw -h
send a raw request and print response
Usage: pldmtool raw [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -m,--mctp_eid UINT          MCTP endpoint ID
  -v,--verbose
  -d,--data UINT ... REQUIRED raw data
```

pldmtool request message format:

pldmtool raw --data 0x80 <pldmType> <cmdType> <payload>

payload - stream of bytes constructed based on the request message format
          defined for the command type as per the spec.

pldmtool response message format:

<instanceId> <hdrVersion> <pldmType> <cmdType> <completioncode> <payloadResp>

payloadResp - stream of bytes displayed based on the response message format
              defined for the command type as per the spec.

Example:

```
$ pldmtool raw -d 0x80 0x00 0x04 0x00 0x00

Request Message:
08 01 80 00 04 00 00
Response Message:
08 01 00 00 04 00 1d 00 00 00 00 00 00 80

```


## pldmtool verbosity

By default verbose flag is disabled on the pldmtool.

Enable verbosity with -v flag as shown below

Example:

pldmtool base GetPLDMTypes -v
