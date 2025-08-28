# BIOS Support

Redfish supports the BIOS Attribute Registry, which provides users with a list
of BIOS attributes supported in the BIOS configuration. To incorporate BIOS
attribute registry support in openBMC, BmcWeb is designed to read data from the
Base BIOS Table. PLDM populates the Base BIOS Table for the BIOS Config Manager
based on BIOS JSON files. BIOS functionality is integrated into PLDM according
to the guidelines in the
[PLDM BIOS Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0247_1.0.0.pdf).
BIOS attributes, also referred to as BIOS parameters or configuration settings,
are structured within JSON files. Each attribute is defined by its name, type,
and type-specific metadata. PLDM parses
[BIOS JSON file](https://github.com/openbmc/pldm/tree/master/oem/ibm/configurations/bios/com.ibm.Hardware.Chassis.Model.Rainier2U)
and creates the
[Base BIOS Table](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/BIOSConfig/Manager.interface.yaml#L62)
hosted by [BIOS Config Manager](https://github.com/openbmc/bios-settings-mgr).
The design is documented in
[DesignDoc](https://github.com/openbmc/docs/blob/master/designs/remote-bios-configuration.md).
Redfish supports
[BIOS Attribute registry](https://redfish.dmtf.org/schemas/v1/AttributeRegistry.v1_3_8.json),
which provides users with the list of BIOS attributes supported in the BIOS
configuration. The BIOS Attribute registry data is supposed to be derived from
[Base BIOS Table](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/BIOSConfig/Manager.interface.yaml#L62).
PLDM populates the Base BIOS Table for BIOS Config Manager based on BIOS Json
files.

After the JSON files are parsed, pldm would also create the string table,
attribute table, and attribute value table as per Section7 in
[PLDM BIOS Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0247_1.0.0.pdf).
Those BIOS tables are exchanged with remote PLDM terminus using the
`GetBiosTable` command as defined in DSP0247_1.0.0.pdf Section 8.1. All the
`bios attribute json files are kept under OEM`
[Path](https://github.com/openbmc/pldm/tree/master/oem). BIOS json configuration
is provided in bios_attr.json file which contains attributes of type enum,
integer and string.

Integer Attribute details as documented at Table9 of
[PLDM BIOS Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0247_1.0.0.pdf):

```json
{
  "entries": [
    {
      "attribute_type": "integer",
      "attribute_name": "Attribute Name",
      "lower_bound": "The lower bound on the integer value",
      "upper_bound": "The upper bound on the integer value",
      "scalar_increment": "The scalar value that is used for the increments to this integer ",
      "default_value": "The default value of the integer",
      "help_text": "Help text about attribute usage",
      "display_name": "Attribute Display Name",
      "read_only": "Read only Attribute"
    }
  ]
}
```

Enum Attribute details as documented at Table6 of
[PLDM BIOS Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0247_1.0.0.pdf):

```json
{
  "entries": [
    {
      "attribute_type": "enum",
      "attribute_name": "Attribute Name",
      "possible_values": [
        "An array of character strings of variable to indicate the possible values of the BIOS attribute"
      ],
      "default_values": "Default value",
      "help_text": "Help text about attribute usage",
      "display_name": "Display Name",
      "read_only": "Read only Attribute"
    }
  ]
}
```

String Attribute details as documented at Table7 of
[PLDM BIOS Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0247_1.0.0.pdf):

```json
{
  "entries": [
    {
      "attribute_type": "string",
      "attribute_name": "Attribute Name",
      "string_type": "It specifies the character encoding format, which can be ASCII, Hex, UTF-8, UTF-16LE, or UTF-16BE. Currently, only ASCII is supported",
      "minimum_string_length": "The minimum length of the string in bytes",
      "maximum_string_length": "The maximum length of the string in bytes",
      "default_string": "The default string itself",
      "help_text": "Help text about attribute usage",
      "display_name": "Attribute Display Name",
      "read_only": "Read only Attribute"
    }
  ]
}
```

As PLDM BIOS Attributes may differ across platforms and systems, supporting
system-specific BIOS attributes is crucial. To achieve this, BIOS JSON files are
organized under folders named after the system type. System type information is
retrieved from the Entity Manager service, which hosts the
[Compatible Interface](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/Inventory/Decorator/Compatible.interface.yaml).

This interface dynamically populates the Names property with system type
information. However, determining the system type in the application space may
take some time since the compatible interface and the Names property are
dynamically created by the Entity Manager. Consequently, BIOS tables are lazily
constructed upon receiving the system type.

To enable system-specific BIOS attribute support within PLDM, the meson option
`system-specific-bios-json` can be utilized. With `system-specific-bios-json`
option `enabled` BIOS JSON files specific to the system type are fetched during
runtime.
