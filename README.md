# Code Organization
At a high-level, code in this repository belongs to one of the following three
components.

## libpldm
This is a library which deals with the encoding and decoding of PLDM messages.
It should be possible to use this library by projects other than OpenBMC, and
hence certain constraints apply to it:
- keeping it light weight
- implementation in C
- minimal dynamic memory allocations
- endian-safe
- no OpenBMC specific dependencies

Source files are named according to the PLDM Type, for eg base.[h/c], fru.[h/c],
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

## libpldmresponder
This library provides handlers for incoming PLDM request messages. It provides
for a registration as well as a plug-in mechanism. The library is implemented in
modern C++, and handles OpenBMC's platform specifics.

The handlers are of the form
```
Response handler(Request payload, size_t payloadLen)
```

Source files are named according to the PLDM Type, for eg base.[hpp/cpp],
fru.[hpp/cpp], etc.

## daemon
This is the PLDM daemon application that deals with various aspects of the
requester and responder functions, as explained at
https://github.com/openbmc/docs/blob/master/designs/pldm-stack.md.

## TODO
Consider hosting libpldm above in a repo of its own, probably even outside the
OpenBMC project? A separate repo would enable something like git submodule.

# Flows
This section documents important code flow paths.

## BMC as PLDM responder
a) PLDM daemon receives PLDM request message from underlying transport.
b) PLDM daemon routes message to message handler, based on the PLDM command.
c) Message handler decodes request paylod into various field(s) of the request
   message. It can make use of a decode_foo_req() API, and doesn't have to
   perform deserialization of the request payload by itself.
d) Message hanlder works with the request field(s) and generates response
   field(s).
e) Message handler prepares a response message. It can make use of an
   encode_foo_resp() API, and doesn't have to perform the serializaiton of the
   response field(s) by itself.

## BMC as PLDM requester
a) A BMC PLDM requester app prepares a PLDM request message. There would be
   several requester apps (based on functionality/PLDM remote device). Each of
   them needn't bother with the serialization of requst field(s), and can
   instead make use of an encode_foo_req() API.
b) BMC requester app requests PLDM daemon to send the request message to remote
   PLDM device.
c) Once the PLDM daemon receives a corresponding response message, it notifies
   the requester app.
d) The requester app has to work with the response field(s). It can make use of
   a decode_foo_resp() API to deserialize the response message.
