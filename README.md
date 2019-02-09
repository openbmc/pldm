# Code Organization
At a high-level code in this repository belongs to one of the following three
pieces.

## libpldm
This is a library which deals with the encoding and decoding of PLDM messages.
It should be possible to use this library by projects other than OpenBMC, and
hence certain constraints apply to it:
- keeping it light weight
- implementation in C
- minimal dynamic memory allocations 
- Endian-safe
- No OpenBMC specific dependencies

Source files are names according to the PLDM Type, for eg base.[h/c], fru.[h/c],
etc.

Given a PLDM command "foo", the library will provide the following API:
For the Requester function:
```
encode_foo_req() - encode a foo request
decode_foo_resp() - decode a response to foo
```
For the Responder function:
```
decode_foo_req() - decode a foo request
encode_foo_resp() - encode a response to foo
```
The library also provides API to pack and unpack PLDM headers.

## libresponder
This library provides handlers for incoming PLDM request messages. It provides
for a registration as well as a plug-in mechanism. The library is implemented in
modern C++, and handles OpenBMC's platform specifics.

Source files are names according to the PLDM Type, for eg base.[hpp/cpp],
fru.[hpp/cpp], etc.

## daemon
This is the PLDM daemon application that deals with various aspects of the
requester and responder functions, as explained at
https://github.com/openbmc/docs/blob/master/designs/pldm-stack.md.

## TODO
Consider hosting libpldm above in a repo of its own, probably even outside the
OpenBMC project? A separate repo would enable something like git submodule.
