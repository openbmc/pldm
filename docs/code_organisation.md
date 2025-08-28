# Code Organization

At a high-level, code in this repository belongs to one of the following three
components.

## libpldmresponder

This library provides handlers for incoming PLDM request messages. It provides
for a registration as well as a plug-in mechanism. The library is implemented in
modern C++, and handles OpenBMC's platform specifics.

The handlers are of the form

```c
Response handler(Request payload, size_t payloadLen)
```

Source files are named according to the PLDM Type, for eg base.[hpp/cpp],
fru.[hpp/cpp], etc.

## OEM/vendor-specific functions

This will support OEM or vendor-specific functions and semantic information.
Following directory structure has to be used:

```txt
    pldm repo
     |---- oem
            |----<oem_name>
                      |----libpldmresponder
                            |---<oem based handler files>

```

<oem_name> - This folder must be created with the name of the OEM/vendor in
lower case. Folders named libpldm and libpldmresponder must be created under the
folder <oem_name>

Files having the oem functionality for the libpldmresponder library should be
placed under the folder oem/<oem_name>/libpldmresponder. They must be adhering
to the rules mentioned under the libpldmresponder section above.

Once the above is done a meson option has to be created in
`pldm/meson_options.txt` with its mapped compiler flag to enable conditional
compilation.

For consistency would recommend using "oem-<oem_name>".

The `pldm/meson.build` and the corresponding source file(s) will need to
incorporate the logic of adding its mapped compiler flag to allow conditional
compilation of the code.

## libpldm

pldm daemon links against the libpldm library during compilation, For more
information on libpldm please refer to
[libpldm](https://github.com/openbmc/libpldm)

## pldmtool

For more information on pldmtool please refer to
[pldmtool](../pldmtool/README.md).
