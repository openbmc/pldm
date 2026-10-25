# Component-targeted firmware update integration tests

## Scope

Verify that `StartUpdate` on a component software object updates only that
component, even when the supplied PLDM package contains sibling components.
Starting on a device object must still select all applicable components in its
matching device record. A package update must still process all matching devices.

The selection is the intersection of the target component identifier and the
matching record's `ApplicableComponents` bitmap. An image elsewhere in the
package is not sufficient. An empty intersection must fail before `RequestUpdate`.
Device refusal of a partial image set must fail without retrying sibling images.

This plan describes hardware validation to perform; it is not a record of
completed tests. Unit tests cover package intake, selected payloads, progress,
missing/non-applicable components and reuse after failure.

## Setup and evidence

1. Record the tested PLDM revision, build options, BMC image and platform. Use a
   device with at least two components and, for the successful component-only
   cases, firmware that accepts partial updates. Keep a second EID available
   for the bundle regression.
2. Discover the current EIDs and software objects. Do not reuse generated
   inventory paths from a previous update or boot:

   ```sh
   busctl --list tree xyz.openbmc_project.PLDM
   pldmtool fw_update GetFirmwareParameters -m "$TEST_EID"
   pldmtool fw_update GetStatus -m "$TEST_EID"
   ```

   For example, a two-component CPU may expose `Device_16`,
   `Device_16_Component_2` and `Device_16_Component_513` objects, each with a
   generated suffix. Discover the corresponding Redfish FirmwareInventory
   members if using Redfish to submit the update.
3. Record every component's active and pending versions and comparison stamps
   on both EIDs. Begin with the FD idle and no unrelated pending activation.
   `ActivateFirmware` is device-scoped; selecting a component does not make
   that PLDM command a component-scoped activation operation.
4. Prepare a valid, signed package accepted by the platform, containing two
   applicable images with known identifiers, versions, lengths and hashes.
   Put the component selected in the second test after its sibling in the
   package. Prepare missing-component and non-applicable-component packages
   whose headers/checksums are otherwise valid, and a signed bundle matching
   both EIDs. Record hashes and decoded package records for every input.
5. Start PLDM packet capture and save the journal from before submission through
   completion and inventory refresh. Capture the returned activation path,
   its state and progress. Packet capture must show command payloads, not just
   command names. Use a transport capture or the platform's PLDM verbose trace.
6. Submit exactly one target per request. The D-Bus entry point is that object's
   `xyz.openbmc_project.Software.Update.StartUpdate`, passing the package FD and
   a supported apply time. Alternatively, submit a Redfish multipart update
   with a single component FirmwareInventory URI in `UpdateParameters.Targets`.
   Use an empty `Targets` array for the bundle case. Confirm the requested
   object is the one whose D-Bus method was called.

## Selection matrix

| Case | Target and package | Required result |
| --- | --- | --- |
| C1 | First component; both images applicable | RequestUpdate reports one component. PassComponentTable has only the selected identifier and START_AND_END. UpdateComponent and firmware data cover only that image. |
| C2 | Second component; same package | Same isolation as C1. Firmware data comes from the second image's original package offset, not the first image. |
| C3 | Device object; same package | Both applicable components are offered in package order. RequestUpdate reports two and PassComponentTable uses START then END. |
| C4 | Component absent from the package | Activation becomes Invalid; no RequestUpdate, PassComponentTable, UpdateComponent or image data is sent. No sibling fallback. |
| C5 | Component present but excluded by the matching device record's bitmap | Same result as C4, even if another device record includes the component. |
| C6 | Component; package with no matching device descriptors | Invalid before PLDM update traffic. |
| C7 | Component; FD rejects a partial/incomplete image set | Failed outcome, no sibling offered or transferred. Record the actual rejection command and code. Recover the FD to IDLE before the next case. |
| C8 | Repeat C2 after failure, then after success | Each attempt selects only that component; progress starts afresh and no stale package/selection is used. |
| C9 | Bundle containing two matching EIDs | Each EID receives all components applicable to its own record. Neither the preceding component request nor its filtered record changes the bundle. |
| C10 | Device update after C2, then C2 after a device update | Device still offers both images; component still offers one. |

For C1 and C2, drive transfer, verification and apply to completion on a device
that accepts partial updates. Record terminal activation state and 100 percent
progress. Capture GetFirmwareParameters before any reset and after the required
activation action: the selected component changes as expected, the sibling's
active/pending versions and stamps remain unchanged, and the other EID is
unchanged. Verify transmitted payload hashes as well as versions; unchanged
versions alone cannot prove that a sibling was not reflashed.

If the CPU refuses partial updates, C7 is the expected hardware result. It does
not establish successful C1/C2 coverage. Complete those cases with a suitable
FD or protocol emulator that supports partial updates. Do not weaken selection
to get a successful CPU update.

## Recovery and acceptance

After a rejected or interrupted attempt, inspect GetStatus. If cancellation is
needed, capture it and wait for IDLE before continuing. A stale DOWNLOAD/VERIFY
state or a crash is a failure requiring investigation, not a reason to silently
retry the complete image set. Rediscover inventory paths after each refresh.

Retain the package manifests/hashes, target URIs, decoded PLDM exchanges,
activation/progress history, journal and before/after firmware parameters for
every case. Mark each case passed, failed or untested with its reason. Do not
infer a pass from task completion or from the number of log entries alone.
