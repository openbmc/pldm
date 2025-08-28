# Flows

This section documents important code flow paths.

## BMC as PLDM responder

a) PLDM daemon receives PLDM request message from underlying transport (MCTP).

b) PLDM daemon routes message to message handler, based on the PLDM command.

c) Message handler decodes request payload into various field(s) of the request
message. It can make use of a decode_foo_req() API, and doesn't have to perform
deserialization of the request payload by itself.

d) Message handler works with the request field(s) and generates response
field(s).

e) Message handler prepares a response message. It can make use of an
encode_foo_resp() API, and doesn't have to perform the serialization of the
response field(s) by itself.

f) The PLDM daemon sends the response message prepared at step e) to the remote
PLDM device.

## BMC as PLDM requester

a) A BMC PLDM requester app prepares a PLDM request message. There would be
several requester apps (based on functionality/PLDM remote device). Each of them
needn't bother with the serialization of request field(s), and can instead make
use of an encode_foo_req() API.

b) BMC requester app requests PLDM daemon to send the request message to remote
PLDM device.

c) Once the PLDM daemon receives a corresponding response message, it notifies
the requester app.

d) The requester app has to work with the response field(s). It can make use of
a decode_foo_resp() API to deserialize the response message.
