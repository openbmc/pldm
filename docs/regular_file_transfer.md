# Regular-file transfers on Phoenix

This implementation supports regular files. FIFO and directory modes are
rejected explicitly. The OEM Arm Device File state sensor remains the trigger;
file data uses standard PLDM type 7 and Base MultipartReceive.

## Flow

Previously: event -> detached worker (busy events dropped) -> Open -> stream
unverified parts -> Close -> temporary archive -> Notify submission -> cleanup.

Now: event -> bounded/coalescing worker queue -> resolve the current terminus
and discovered file -> validate size/range -> negotiate type-7 part size ->
DfOpen -> heartbeat -> bounded sections with cumulative CRC validation ->
nonblocking socket delivery. For internal crashlog collection, the last section
is delivered before its completion ACK; the sink synchronizes its file and
parent directory, then releases the ACK and DfClose. The worker checks the
terminal protocol result, builds/synchronizes an archive, submits Notify, and
checks the matching SourceDumpId and Common.Progress status. Failures retain
private work files. Successful collections retain a synchronized local archive.

The early sink synchronization matters on this MCP firmware: a successful
DfClose of all files can trigger host recovery and release the source buffers.
An ACK or successful socket EOF alone is insufficient evidence of storage.

## Interfaces and policy

Existing File.Open and pldmtool file command names/options remain available.
File.Open returns a stream FD; its existing ABI has no per-operation terminal
status. Internal consumers receive a completion future and storage promise. The
CLI verifies byte count and output writes, but cannot prove remote close success
through this legacy ABI. Adding such a public API requires a shared
phosphor-dbus-interfaces proposal, rather than a private property.

DSP0242 1.0.0 supplies DfOpen/Close/Heartbeat; DSP0240 1.2.0 section 9.6.6
supplies the multipart state machine. Negotiated part sizes include PLDM
headers, fixed response fields and CRC. Nonfinal parts fill the negotiated
payload capacity. CRC retries retain the current handle and are bounded.
Heartbeat support is required by this client policy; a peer without the optional
command is not thereby declared nonconforming. There is no legacy
unnegotiated-transfer fallback.

Local limits are 64 KiB per section, three CRC retries per part, a two-minute
transfer deadline checked between requests, five seconds without socket
progress, thirty seconds for sink synchronization, and 32 pending collections. A
negotiated heartbeat below 10 ms is rejected as unsupported scheduling.
Transport request timers also bound individual requests. Archive creation is
bounded to sixty seconds. These are implementation policies, not DMTF limits.

Unavailable endpoints cancel active file tasks; removed descriptors remain owned
until cancellation completes. An invalidated descriptor requires fresh PDR
discovery before reuse. The queue checks terminus object identity before
accessing the event-thread requester. Instance-ID quarantine and reset/reuse
fault injection still require investigation; no complete stale-response
protection claim is made.

Private data lives under /var/lib/pldm/crashlogs with restrictive permissions,
unique collection directories, a synchronized persistent BMC collection ID, and
strict provider filename validation. This BMC ID is not an MCP incident ID.
Existing dump IDs are skipped. The legacy /tmp/system-dumps bridge is checked
for ownership and permissions and is exclusively reserved without following
symlinks. Admission leaves a 16 MiB reserve and space for two file copies.
Retained archives require operator cleanup; automated recovery/replay and
retention policy are follow-up work. Filesystem space checks cannot eliminate
concurrent disk exhaustion, which remains a handled transfer failure.

The pinned dump collector reports Completed after copying but does not fsync its
destination. The local archive is therefore retained even on Completed; this
change does not claim power-loss durability for the collector's copy.

## Qualification

Cross-building and standalone tests are prerequisites, not hardware validation.
Before Gerrit submission, test Phoenix normal multi-chip collection, range
reads, malformed/CRC responses, slow and closed readers, storage exhaustion,
close-response loss, discovery races, endpoint removal/reset/EID reuse, repeated
events, queue saturation, daemon stop/restart, and receiver failure. Confirm
that retained files survive a reboot and correlate recovery with final section
ACK/close and persistent storage. Do not push this series before those tests.
