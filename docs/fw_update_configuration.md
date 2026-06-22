# Firmware Update Configuration

PLDM supports firmware updates through two mechanisms:

1. **D-Bus API**: Using the StartUpdate D-Bus interface for firmware updates
2. **Inotify monitoring**: Automatic detection of firmware packages placed in
   `/tmp/images`

The inotify-based firmware update monitoring can be enabled or disabled using
the meson option `fw-update-pkg-inotify`. When enabled, pldmd will automatically
monitor the `/tmp/images` directory for new firmware packages and process them
automatically. When disabled, only D-Bus API-based firmware updates will be
supported. To disable inotify-based firmware update monitoring (default):

```bash
meson setup build -Dfw-update-pkg-inotify=disabled
```

To enable inotify-based firmware update monitoring:

```bash
meson setup build -Dfw-update-pkg-inotify=enabled
```

## Pre and Post Update Condition Services

PLDM firmware update supports optional pre and post update condition services
that can be executed during the firmware update process. These services are
specified in the configuration files and are used for device-specific handling
such as checking conditions before update or performing cleanup/validation after
update.

`PreUpdateTarget` and `PostUpdateTarget` hold a systemd unit name without the
`.service` suffix, which PLDM appends when starting the unit. Omit the property
when a component has no such condition; an empty unit name is rejected by the
[schema](../configurations/fw-update-conditions/schema/schema.json) and is
reported at runtime instead of being silently skipped.

### Condition Service Completion and Timeouts

PLDM starts a condition unit with the systemd `StartUnit` method and waits for
its `JobRemoved` signal, as described in the
[code update design](https://github.com/openbmc/docs/blob/master/designs/code-update.md#pre-and-post-update-conditions).
Only the job result `done` is treated as success; any other result fails the
condition and, for a pre-update condition, aborts the update.

PLDM applies no timeout of its own, because systemd reports every dequeued job
through `JobRemoved` exactly once. A condition unit that can hang is therefore
expected to bound itself with `RuntimeMaxSec`, which makes systemd terminate the
unit and deliver the resulting failure through `JobRemoved`. Without that
setting a hanging condition unit leaves the firmware update waiting
indefinitely.

```ini
[Service]
Type=oneshot
RuntimeMaxSec=60
ExecStart=/usr/bin/example-pre-update
```

### Condition Service Arguments Format

When invoking condition services, PLDM passes arguments that contain both the
configured condition argument and the requested apply time. The format is:

```text
<conditionArg>,applyTime=<applyTimeValue>
```

Where:

- `<conditionArg>`: The condition argument from `TargetArgs` (optional, may be
  empty)
  - For `boardName`, the value is passed as `boardName=<boardName>`
- `<applyTimeValue>`: One of:
  - `Immediate` - Apply the update immediately
  - `OnReset` - Apply the update on next system reset
  - `OnStart` - Apply the update on next system start

### Examples

**Example 1: Post-condition with empty condition argument** If `TargetArgs` does
not provide any condition argument and `applyTime` is `OnReset`:

```text
applyTime=OnReset
```

**Example 2: Post-condition with board name argument** If `TargetArgs` contains
`boardName`, the board name is `board1`, and `applyTime` is `Immediate`:

```text
boardName=board1,applyTime=Immediate
```

### Post-Condition Service Processing

Post-condition services receive these arguments and can use them for conditional
processing. For example:

- Skip system reset if `applyTime` is not `Immediate`
- Perform device-specific validation based on `applyTime`
- Schedule deferred operations based on the apply time value

The service should interpret these parameters to determine appropriate actions
for the given apply time.
