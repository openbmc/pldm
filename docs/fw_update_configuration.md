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
