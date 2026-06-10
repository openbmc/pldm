#!/usr/bin/env python3

"""Script to create PLDM FW update package"""

import argparse
import binascii
import enum
import json
import math
import os
import struct
import sys
from datetime import datetime

from bitarray import bitarray
from bitarray.util import ba2int

string_types = dict(
    [
        ("Unknown", 0),
        ("ASCII", 1),
        ("UTF8", 2),
        ("UTF16", 3),
        ("UTF16LE", 4),
        ("UTF16BE", 5),
    ]
)

initial_descriptor_type_name_length = {
    0x0000: ["PCI Vendor ID", 2],
    0x0001: ["IANA Enterprise ID", 4],
    0x0002: ["UUID", 16],
    0x0003: ["PnP Vendor ID", 3],
    0x0004: ["ACPI Vendor ID", 4],
}

descriptor_type_name_length = {
    0x0000: ["PCI Vendor ID", 2],
    0x0001: ["IANA Enterprise ID", 4],
    0x0002: ["UUID", 16],
    0x0003: ["PnP Vendor ID", 3],
    0x0004: ["ACPI Vendor ID", 4],
    0x0005: ["IEEE Assigned Company ID", 3],
    0x0006: ["SCSI Vendor ID", 8],
    0x0100: ["PCI Device ID", 2],
    0x0101: ["PCI Subsystem Vendor ID", 2],
    0x0102: ["PCI Subsystem ID", 2],
    0x0103: ["PCI Revision ID", 1],
    0x0104: ["PnP Product Identifier", 4],
    0x0105: ["ACPI Product Identifier", 4],
    0x0106: ["ASCII Model Number (Long String)", 40],
    0x0107: ["ASCII Model Number (Short String)", 10],
    0x0108: ["SCSI Product ID", 16],
    0x0109: ["UBM Controller Device Code", 4],
    0x010A: ["IEEE EUI-64 ID", 8],
    0x010B: ["PCI Revision ID Range", 2],
}

expected_package_format_versions = [
    1,  # DSP0267 1.0.0
    2,  # DSP0267 1.1.0
    3,  # DSP0267 1.2.0
    4,  # DSP0267 1.3.0
]


class ComponentOptions(enum.IntEnum):
    """
    Enum to represent ComponentOptions
    """

    ForceUpdate = 0
    UseComponentCompStamp = 1


def check_string_length(string):
    """Check if the length of the string is not greater than 255."""
    if len(string) > 255:
        sys.exit("ERROR: Max permitted string length is 255")


def write_pkg_release_date_time(pldm_fw_up_pkg, release_date_time):
    """
    Write the timestamp into the package header. The timestamp is formatted as
    series of 13 bytes defined in DSP0240 specification.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            release_date_time: Package Release Date Time
    """
    time = release_date_time.time()
    date = release_date_time.date()
    us_bytes = time.microsecond.to_bytes(3, byteorder="little")
    pldm_fw_up_pkg.write(
        struct.pack(
            "<hBBBBBBBBHB",
            0,
            us_bytes[0],
            us_bytes[1],
            us_bytes[2],
            time.second,
            time.minute,
            time.hour,
            date.day,
            date.month,
            date.year,
            0,
        )
    )


def write_package_version_string(pldm_fw_up_pkg, metadata):
    """
    Write PackageVersionStringType, PackageVersionStringLength and
    PackageVersionString to the package header.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            metadata: metadata about PLDM FW update package
    """
    # Hardcoded string type to ASCII
    string_type = string_types["ASCII"]
    package_version_string = metadata["PackageHeaderInformation"][
        "PackageVersionString"
    ]
    check_string_length(package_version_string)
    format_string = "<BB" + str(len(package_version_string)) + "s"
    pldm_fw_up_pkg.write(
        struct.pack(
            format_string,
            string_type,
            len(package_version_string),
            package_version_string.encode("ascii"),
        )
    )


def write_component_bitmap_bit_length(pldm_fw_up_pkg, metadata):
    """
    ComponentBitmapBitLength in the package header indicates the number of bits
    that will be used represent the bitmap in the ApplicableComponents field
    for a matching device. The value shall be a multiple of 8 and be large
    enough to contain a bit for each component in the package. The number of
    components in the JSON file is used to populate the bitmap length.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            metadata: metadata about PLDM FW update package

        Returns:
            ComponentBitmapBitLength: number of bits that will be used
            represent the bitmap in the ApplicableComponents field for a
            matching device
    """
    # The script supports up to 32 components now
    max_components = 32
    bitmap_multiple = 8

    num_components = len(metadata["ComponentImageInformationArea"])
    if num_components > max_components:
        sys.exit("ERROR: only up to 32 components supported now")
    component_bitmap_bit_length = bitmap_multiple * math.ceil(
        num_components / bitmap_multiple
    )
    pldm_fw_up_pkg.write(struct.pack("<H", int(component_bitmap_bit_length)))
    return component_bitmap_bit_length


def write_pkg_header_info(pldm_fw_up_pkg, metadata):
    """
    ComponentBitmapBitLength in the package header indicates the number of bits
    that will be used represent the bitmap in the ApplicableComponents field
    for a matching device. The value shall be a multiple of 8 and be large
    enough to contain a bit for each component in the package. The number of
    components in the JSON file is used to populate the bitmap length.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            metadata: metadata about PLDM FW update package

        Returns:
            ComponentBitmapBitLength: number of bits that will be used
            represent the bitmap in the ApplicableComponents field for a
            matching device
    """
    uuid = metadata["PackageHeaderInformation"]["PackageHeaderIdentifier"]
    package_header_identifier = bytearray.fromhex(uuid)
    pldm_fw_up_pkg.write(package_header_identifier)

    package_header_format_revision = metadata["PackageHeaderInformation"][
        "PackageHeaderFormatVersion"
    ]
    # Size will be computed and updated subsequently
    package_header_size = 0
    pldm_fw_up_pkg.write(
        struct.pack("<BH", package_header_format_revision, package_header_size)
    )

    release_date_time_str = metadata["PackageHeaderInformation"].get(
        "PackageReleaseDateTime", None
    )

    if release_date_time_str is not None:
        formats = [
            "%Y-%m-%dT%H:%M:%S",
            "%Y-%m-%d %H:%M:%S",
            "%d/%m/%Y %H:%M:%S",
        ]
        release_date_time = None
        for fmt in formats:
            try:
                release_date_time = datetime.strptime(
                    release_date_time_str, fmt
                )
                break
            except ValueError:
                pass
        if release_date_time is None:
            sys.exit("Can't parse release date '%s'" % release_date_time_str)
    else:
        release_date_time = datetime.now()

    write_pkg_release_date_time(pldm_fw_up_pkg, release_date_time)

    component_bitmap_bit_length = write_component_bitmap_bit_length(
        pldm_fw_up_pkg, metadata
    )
    write_package_version_string(pldm_fw_up_pkg, metadata)
    return package_header_format_revision, component_bitmap_bit_length


def get_applicable_components(device, components, component_bitmap_bit_length):
    """
    This function figures out the components applicable for the device and sets
    the ApplicableComponents bitfield accordingly.

        Parameters:
            device: device information
            components: list of components in the package
            component_bitmap_bit_length: length of the ComponentBitmapBitLength

        Returns:
            The ApplicableComponents bitfield
    """
    applicable_components_list = device["ApplicableComponents"]
    applicable_components = bitarray(
        component_bitmap_bit_length, endian="little"
    )
    applicable_components.setall(0)
    for component_index in applicable_components_list:
        if 0 <= component_index < len(components):
            applicable_components[component_index] = 1
        else:
            sys.exit("ERROR: Applicable Component index not found.")
    return applicable_components


def prepare_record_descriptors(descriptors):
    """
    This function processes the Descriptors and prepares the RecordDescriptors
    section of the the firmware device ID record.

        Parameters:
            descriptors: Descriptors entry

        Returns:
            RecordDescriptors, DescriptorCount
    """
    record_descriptors = bytearray()
    vendor_defined_desc_type = 65535
    vendor_desc_title_str_type_len = 1
    vendor_desc_title_str_len_len = 1
    descriptor_count = 0

    for descriptor in descriptors:
        descriptor_type = descriptor["DescriptorType"]
        if descriptor_count == 0:
            if (
                initial_descriptor_type_name_length.get(descriptor_type)
                is None
            ):
                sys.exit("ERROR: Initial descriptor type not supported")
        else:
            if (
                descriptor_type_name_length.get(descriptor_type) is None
                and descriptor_type != vendor_defined_desc_type
            ):
                sys.exit("ERROR: Descriptor type not supported")

        if descriptor_type == vendor_defined_desc_type:
            vendor_desc_title_str = descriptor[
                "VendorDefinedDescriptorTitleString"
            ]
            vendor_desc_data = descriptor["VendorDefinedDescriptorData"]
            check_string_length(vendor_desc_title_str)
            vendor_desc_title_str_type = string_types["ASCII"]
            descriptor_length = (
                vendor_desc_title_str_type_len
                + vendor_desc_title_str_len_len
                + len(vendor_desc_title_str)
                + len(bytearray.fromhex(vendor_desc_data))
            )
            format_string = "<HHBB" + str(len(vendor_desc_title_str)) + "s"
            record_descriptors.extend(
                struct.pack(
                    format_string,
                    descriptor_type,
                    descriptor_length,
                    vendor_desc_title_str_type,
                    len(vendor_desc_title_str),
                    vendor_desc_title_str.encode("ascii"),
                )
            )
            record_descriptors.extend(bytearray.fromhex(vendor_desc_data))
            descriptor_count += 1
        else:
            descriptor_type = descriptor["DescriptorType"]
            descriptor_data = descriptor["DescriptorData"]
            descriptor_length = len(bytearray.fromhex(descriptor_data))
            if (
                descriptor_length
                != descriptor_type_name_length.get(descriptor_type)[1]
            ):
                err_string = (
                    "ERROR: Descriptor type - "
                    + descriptor_type_name_length.get(descriptor_type)[0]
                    + " length is incorrect"
                )
                sys.exit(err_string)
            format_string = "<HH"
            record_descriptors.extend(
                struct.pack(format_string, descriptor_type, descriptor_length)
            )
            record_descriptors.extend(bytearray.fromhex(descriptor_data))
            descriptor_count += 1
    return record_descriptors, descriptor_count


def write_fw_device_identification_area(
    pldm_fw_up_pkg,
    metadata,
    package_header_format_revision,
    component_bitmap_bit_length,
):
    """
    Write firmware device ID records into the PLDM package header

    This function writes the DeviceIDRecordCount and the
    FirmwareDeviceIDRecords into the firmware update package by processing the
    metadata JSON. Currently there is no support for optional
    FirmwareDevicePackageData and ReferenceManifestData.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            metadata: metadata about PLDM FW update package
            package_header_format_revision: Format Revision of the package
            component_bitmap_bit_length: length of the ComponentBitmapBitLength
    """
    # The spec limits the number of firmware device ID records to 255
    max_device_id_record_count = 255
    devices = metadata["FirmwareDeviceIdentificationArea"]
    device_id_record_count = len(devices)
    if device_id_record_count > max_device_id_record_count:
        sys.exit(
            "ERROR: there can be only up to 255 entries in the                "
            " FirmwareDeviceIdentificationArea section"
        )

    # DeviceIDRecordCount
    pldm_fw_up_pkg.write(struct.pack("<B", device_id_record_count))

    for device in devices:
        # RecordLength size
        record_length = 2

        # DescriptorCount
        record_length += 1

        # DeviceUpdateOptionFlags
        device_update_option_flags = bitarray(32, endian="little")
        device_update_option_flags.setall(0)
        # Continue component updates after failure
        supported_device_update_option_flags = [0]
        for option in device["DeviceUpdateOptionFlags"]:
            if option not in supported_device_update_option_flags:
                sys.exit("ERROR: unsupported DeviceUpdateOptionFlag entry")
            device_update_option_flags[option] = 1
        record_length += 4

        # ComponentImageSetVersionStringType supports only ASCII for now
        component_image_set_version_string_type = string_types["ASCII"]
        record_length += 1

        # ComponentImageSetVersionStringLength
        component_image_set_version_string = device[
            "ComponentImageSetVersionString"
        ]
        check_string_length(component_image_set_version_string)
        record_length += len(component_image_set_version_string)
        record_length += 1

        # Optional FirmwareDevicePackageData not supported now,
        # FirmwareDevicePackageDataLength is set to 0x0000
        fw_device_pkg_data_length = 0
        record_length += 2

        # Optional ReferenceManifestData - read from JSON if provided
        reference_manifest_data = None
        reference_manifest_length = 0
        if package_header_format_revision >= 4:
            if "ReferenceManifestData" in device:
                reference_manifest_data_str = device["ReferenceManifestData"]
                reference_manifest_data = bytearray.fromhex(
                    reference_manifest_data_str
                )
                reference_manifest_length = len(reference_manifest_data)
                record_length += reference_manifest_length
            record_length += 4

        # ApplicableComponents
        components = metadata["ComponentImageInformationArea"]
        applicable_components = get_applicable_components(
            device, components, component_bitmap_bit_length
        )
        applicable_components_bitfield_length = round(
            len(applicable_components) / 8
        )
        record_length += applicable_components_bitfield_length

        # RecordDescriptors
        descriptors = device["Descriptors"]
        record_descriptors, descriptor_count = prepare_record_descriptors(
            descriptors
        )
        record_length += len(record_descriptors)

        if 1 <= package_header_format_revision <= 3:
            format_string = (
                "<HBIBBH"
                + str(applicable_components_bitfield_length)
                + "s"
                + str(len(component_image_set_version_string))
                + "s"
            )
            pldm_fw_up_pkg.write(
                struct.pack(
                    format_string,
                    record_length,
                    descriptor_count,
                    ba2int(device_update_option_flags),
                    component_image_set_version_string_type,
                    len(component_image_set_version_string),
                    fw_device_pkg_data_length,
                    applicable_components.tobytes(),
                    component_image_set_version_string.encode("ascii"),
                )
            )
        elif package_header_format_revision >= 4:
            format_string = (
                "<HBIBBHI"
                + str(applicable_components_bitfield_length)
                + "s"
                + str(len(component_image_set_version_string))
                + "s"
            )
            pldm_fw_up_pkg.write(
                struct.pack(
                    format_string,
                    record_length,
                    descriptor_count,
                    ba2int(device_update_option_flags),
                    component_image_set_version_string_type,
                    len(component_image_set_version_string),
                    fw_device_pkg_data_length,
                    reference_manifest_length,
                    applicable_components.tobytes(),
                    component_image_set_version_string.encode("ascii"),
                )
            )
        pldm_fw_up_pkg.write(record_descriptors)
        # Write ReferenceManifestData if provided (only for format version >= 4)
        if (
            package_header_format_revision >= 4
            and reference_manifest_data is not None
        ):
            pldm_fw_up_pkg.write(reference_manifest_data)


def write_downstream_device_identification_area(
    pldm_fw_up_pkg,
    metadata,
    package_header_format_revision,
    component_bitmap_bit_length,
):
    """
    Write downstream device ID records into the PLDM package header

    This function writes the DownstreamDeviceIDRecordCount and the
    DownstreamDeviceIDRecords into the firmware update package by processing the
    metadata JSON. Currently there is no support for optional
    DownstreamDevicePackageData and DownstreamDeviceReferenceManifestData.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            metadata: metadata about PLDM FW update package
            component_bitmap_bit_length: length of the ComponentBitmapBitLength
    """
    # The spec limits the number of firmware device ID records to 255
    max_downstream_device_id_record_count = 255
    # check if metadata["DownstreamDeviceIdentificationArea"] exists first
    if "DownstreamDeviceIdentificationArea" not in metadata.keys():
        downstream_device_id_record_count = 0
        pldm_fw_up_pkg.write(
            struct.pack("<B", downstream_device_id_record_count)
        )
        return
    downstream_devices = metadata["DownstreamDeviceIdentificationArea"]
    downstream_device_id_record_count = len(downstream_devices)
    if (
        downstream_device_id_record_count
        > max_downstream_device_id_record_count
    ):
        sys.exit(
            "ERROR: there can be only upto 255 entries in the                "
            " FirmwareDeviceIdentificationArea section"
        )

    # DeviceIDRecordCount
    pldm_fw_up_pkg.write(struct.pack("<B", downstream_device_id_record_count))

    for downstream_device in downstream_devices:
        # RecordLength size
        downstream_device_record_length = 2

        # DescriptorCount
        downstream_device_record_length += 1

        # DownstreamDeviceUpdateOptionFlags
        downstream_device_update_option_flags = bitarray(32, endian="little")
        downstream_device_update_option_flags.setall(0)
        # Continue component updates after failure
        supported_downstream_device_update_option_flags = [0]
        for option in downstream_device["DownstreamDeviceUpdateOptionFlags"]:
            if option not in supported_downstream_device_update_option_flags:
                sys.exit(
                    "ERROR: unsupported DownstreamDeviceUpdateOptionFlag entry"
                )
            downstream_device_update_option_flags[option] = 1
        downstream_device_record_length += 4

        # ComponentImageSetVersionStringType supports only ASCII for now
        downstream_device_self_contained_activation_min_version_string_type = (
            string_types["ASCII"]
        )
        downstream_device_record_length += 1

        if downstream_device_update_option_flags[0]:
            # Downstream Device can support self-contained activation with minimal version level defined by
            # DownstreamDeviceSelfContainedActivationMinVersion fields

            downstream_device_self_contained_activation_min_version_string = (
                downstream_device[
                    "DownstreamDeviceSelfContainedActivationMinVersionString"
                ]
            )
            check_string_length(
                downstream_device_self_contained_activation_min_version_string
            )
            downstream_device_record_length += len(
                downstream_device_self_contained_activation_min_version_string
            )
            downstream_device_self_contained_activation_min_version_comparison_stamp = downstream_device[
                "DownstreamDeviceSelfContainedActivationMinVersionComparisonStamp"
            ]
            downstream_device_record_length += 4
        else:
            downstream_device_self_contained_activation_min_version_string_type = (
                0
            )
            downstream_device_self_contained_activation_min_version_string = ""
        # DownstreamDeviceSelfContainedActivationMinVersionStringLength
        downstream_device_record_length += 1

        # Optional DownstreamDevicePackageData not supported now,
        # DownstreamDevicePackageDataLength is set to 0x0000
        downstream_device_pkg_data_length = 0
        downstream_device_record_length += 2

        # Optional DownstreamDeviceReferenceManifestData - read from JSON if provided
        downstream_device_reference_manifest_data = None
        downstream_device_reference_manifest_length = 0
        if package_header_format_revision >= 4:
            if "DownstreamDeviceReferenceManifestData" in downstream_device:
                downstream_device_reference_manifest_data_str = (
                    downstream_device["DownstreamDeviceReferenceManifestData"]
                )
                downstream_device_reference_manifest_data = bytearray.fromhex(
                    downstream_device_reference_manifest_data_str
                )
                downstream_device_reference_manifest_length = len(
                    downstream_device_reference_manifest_data
                )
                downstream_device_record_length += (
                    downstream_device_reference_manifest_length
                )
            downstream_device_record_length += 4

        # ApplicableComponents
        components = metadata["ComponentImageInformationArea"]
        downstream_device_applicable_components = get_applicable_components(
            downstream_device, components, component_bitmap_bit_length
        )
        downstream_device_applicable_components_bitfield_length = round(
            len(downstream_device_applicable_components) / 8
        )
        downstream_device_record_length += (
            downstream_device_applicable_components_bitfield_length
        )

        # RecordDescriptors
        descriptors = downstream_device["Descriptors"]
        record_descriptors, descriptor_count = prepare_record_descriptors(
            descriptors
        )
        downstream_device_record_length += len(record_descriptors)
        if package_header_format_revision <= 3:
            format_string = (
                "<HBIBBH"
                + str(downstream_device_applicable_components_bitfield_length)
                + "s"
                + str(
                    len(
                        downstream_device_self_contained_activation_min_version_string
                    )
                )
                + "s"
            )
            pldm_fw_up_pkg.write(
                struct.pack(
                    format_string,
                    downstream_device_record_length,
                    descriptor_count,
                    ba2int(downstream_device_update_option_flags),
                    downstream_device_self_contained_activation_min_version_string_type,
                    len(
                        downstream_device_self_contained_activation_min_version_string
                    ),
                    downstream_device_pkg_data_length,
                    downstream_device_applicable_components.tobytes(),
                    downstream_device_self_contained_activation_min_version_string.encode(
                        "ascii"
                    ),
                )
            )
        elif package_header_format_revision >= 4:
            format_string = (
                "<HBIBBHI"
                + str(downstream_device_applicable_components_bitfield_length)
                + "s"
                + str(
                    len(
                        downstream_device_self_contained_activation_min_version_string
                    )
                )
                + "s"
            )
            pldm_fw_up_pkg.write(
                struct.pack(
                    format_string,
                    downstream_device_record_length,
                    descriptor_count,
                    ba2int(downstream_device_update_option_flags),
                    downstream_device_self_contained_activation_min_version_string_type,
                    len(
                        downstream_device_self_contained_activation_min_version_string
                    ),
                    downstream_device_pkg_data_length,
                    downstream_device_reference_manifest_length,
                    downstream_device_applicable_components.tobytes(),
                    downstream_device_self_contained_activation_min_version_string.encode(
                        "ascii"
                    ),
                )
            )
        if downstream_device_update_option_flags[0]:
            pldm_fw_up_pkg.write(
                struct.pack(
                    "<I",
                    downstream_device_self_contained_activation_min_version_comparison_stamp,
                )
            )
        pldm_fw_up_pkg.write(record_descriptors)
        # Write DownstreamDeviceReferenceManifestData if provided (only for format version >= 4)
        if (
            package_header_format_revision >= 4
            and downstream_device_reference_manifest_data is not None
        ):
            pldm_fw_up_pkg.write(downstream_device_reference_manifest_data)


def get_component_comparison_stamp(component):
    """
    Get component comparison stamp from metadata file.

    This function checks if ComponentOptions field is having value 1. For
    ComponentOptions 1, ComponentComparisonStamp value from metadata file
    is used and Default value 0xFFFFFFFF is used for other Component Options.

    Parameters:
        component: Component image info
    Returns:
        component_comparison_stamp: Component Comparison stamp
    """
    component_comparison_stamp = 0xFFFFFFFF
    if (
        int(ComponentOptions.UseComponentCompStamp)
        in component["ComponentOptions"]
    ):
        # Use FD vendor selected value from metadata file
        if "ComponentComparisonStamp" not in component.keys():
            sys.exit(
                "ERROR: ComponentComparisonStamp is required"
                " when value '1' is specified in ComponentOptions field"
            )
        else:
            try:
                tmp_component_cmp_stmp = int(
                    component["ComponentComparisonStamp"], 16
                )
                if 0 < tmp_component_cmp_stmp < 0xFFFFFFFF:
                    component_comparison_stamp = tmp_component_cmp_stmp
                else:
                    sys.exit(
                        "ERROR: Value for ComponentComparisonStamp "
                        " should be  [0x01 - 0xFFFFFFFE] when "
                        "ComponentOptions bit is set to"
                        "'1'(UseComponentComparisonStamp)"
                    )
            except ValueError:  # invalid hext format
                sys.exit("ERROR: Invalid hex for ComponentComparisonStamp")
    return component_comparison_stamp


def write_component_image_info_area(
    pldm_fw_up_pkg, metadata, package_header_format_revision, image_files
):
    """
    Write component image information area into the PLDM package header

    This function writes the ComponentImageCount and the
    ComponentImageInformation into the firmware update package by processing
    the metadata JSON. Currently, there is no support for ComponentOpaqueData.

    Parameters:
        pldm_fw_up_pkg: PLDM FW update package
        metadata: metadata about PLDM FW update package
        image_files: component images
    """
    components = metadata["ComponentImageInformationArea"]
    # ComponentImageCount
    pldm_fw_up_pkg.write(struct.pack("<H", len(components)))
    component_location_offsets = []
    # ComponentLocationOffset position in individual component image
    # information
    component_location_offset_pos = 12

    for component in components:
        # Record the location of the ComponentLocationOffset to be updated
        # after appending images to the firmware update package
        component_location_offsets.append(
            pldm_fw_up_pkg.tell() + component_location_offset_pos
        )

        # ComponentClassification
        component_classification = component["ComponentClassification"]
        if component_classification < 0 or component_classification > 0xFFFF:
            sys.exit(
                "ERROR: ComponentClassification should be [0x0000 - 0xFFFF]"
            )

        # ComponentIdentifier
        component_identifier = component["ComponentIdentifier"]
        if component_identifier < 0 or component_identifier > 0xFFFF:
            sys.exit("ERROR: ComponentIdentifier should be [0x0000 - 0xFFFF]")

        # ComponentComparisonStamp
        component_comparison_stamp = get_component_comparison_stamp(component)

        # ComponentOptions
        component_options = bitarray(16, endian="little")
        component_options.setall(0)
        supported_component_options = [0, 1, 2]
        for option in component["ComponentOptions"]:
            if option not in supported_component_options:
                sys.exit(
                    "ERROR: unsupported ComponentOption in                   "
                    " ComponentImageInformationArea section"
                )
            component_options[option] = 1

        # RequestedComponentActivationMethod
        requested_component_activation_method = bitarray(16, endian="little")
        requested_component_activation_method.setall(0)
        supported_requested_component_activation_method = [0, 1, 2, 3, 4, 5]
        for option in component["RequestedComponentActivationMethod"]:
            if option not in supported_requested_component_activation_method:
                sys.exit(
                    "ERROR: unsupported RequestedComponent                    "
                    "    ActivationMethod entry"
                )
            requested_component_activation_method[option] = 1

        # ComponentLocationOffset
        component_location_offset = 0
        # ComponentSize
        component_size = 0
        # ComponentVersionStringType
        component_version_string_type = string_types["ASCII"]
        # ComponentVersionStringlength
        # ComponentVersionString
        component_version_string = component["ComponentVersionString"]
        check_string_length(component_version_string)

        format_string = "<HHIHHIIBB" + str(len(component_version_string)) + "s"
        pldm_fw_up_pkg.write(
            struct.pack(
                format_string,
                component_classification,
                component_identifier,
                component_comparison_stamp,
                ba2int(component_options),
                ba2int(requested_component_activation_method),
                component_location_offset,
                component_size,
                component_version_string_type,
                len(component_version_string),
                component_version_string.encode("ascii"),
            )
        )
        if package_header_format_revision >= 3:
            # ComponentOpaqueDataLength
            component_opaque_data_length = 0
            # ComponentOpaqueData
            pldm_fw_up_pkg.write(
                struct.pack("<I", component_opaque_data_length)
            )

    index = 0
    pkg_header_checksum_size = 4
    start_offset = pldm_fw_up_pkg.tell() + pkg_header_checksum_size
    if package_header_format_revision >= 4:
        pkg_payload_checksum_size = 4
        start_offset += pkg_payload_checksum_size
    # Update ComponentLocationOffset and ComponentSize for all the components
    for offset in component_location_offsets:
        file_size = os.stat(image_files[index]).st_size
        pldm_fw_up_pkg.seek(offset)
        pldm_fw_up_pkg.write(struct.pack("<II", start_offset, file_size))
        start_offset += file_size
        index += 1
    pldm_fw_up_pkg.seek(0, os.SEEK_END)


def write_pkg_header_checksum(pldm_fw_up_pkg):
    """
    Write PackageHeaderChecksum into the PLDM package header.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
    """
    pldm_fw_up_pkg.seek(0)
    package_header_checksum = binascii.crc32(pldm_fw_up_pkg.read())
    pldm_fw_up_pkg.seek(0, os.SEEK_END)
    pldm_fw_up_pkg.write(struct.pack("<I", package_header_checksum))


def write_pkg_payload_checksum(pldm_fw_up_pkg, image_files):
    """
    Write PackagePayloadChecksum into the PLDM package header.

    Per DSP0267: The integrity checksum of the PLDM Package Payload.
    It is calculated starting at the first byte immediately following this field
    and includes all bytes of the firmware package payload structure, including
    any padding of bytes between component images.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            image_files: component images
    """
    # Calculate CRC32 over the entire payload (all component images)
    # using the IEEE 802.3 CRC-32 polynomial
    package_payload_checksum = 0
    for image in image_files:
        with open(image, "rb") as file:
            # Continue CRC32 calculation across all images
            package_payload_checksum = binascii.crc32(
                file.read(), package_payload_checksum
            )
    # Ensure the checksum is unsigned 32-bit
    package_payload_checksum = package_payload_checksum & 0xFFFFFFFF
    pldm_fw_up_pkg.seek(0, os.SEEK_END)
    pldm_fw_up_pkg.write(struct.pack("<I", package_payload_checksum))


def update_pkg_header_size(pldm_fw_up_pkg, package_header_format_revision):
    """
    Update PackageHeader in the PLDM package header. The package header size
    which is the count of all bytes in the PLDM package header structure is
    calculated once the package header contents is complete.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
    """
    pkg_header_checksum_size = 4
    file_size = pldm_fw_up_pkg.tell() + pkg_header_checksum_size
    if package_header_format_revision >= 4:
        pkg_payload_checksum_size = 4
        file_size += pkg_payload_checksum_size
    pkg_header_size_offset = 17
    # Seek past PackageHeaderIdentifier and PackageHeaderFormatRevision
    pldm_fw_up_pkg.seek(pkg_header_size_offset)
    pldm_fw_up_pkg.write(struct.pack("<H", file_size))
    pldm_fw_up_pkg.seek(0, os.SEEK_END)


def append_component_images(pldm_fw_up_pkg, image_files):
    """
    Append the component images to the firmware update package.

        Parameters:
            pldm_fw_up_pkg: PLDM FW update package
            image_files: component images
    """
    for image in image_files:
        with open(image, "rb") as file:
            for line in file:
                pldm_fw_up_pkg.write(line)


def extract_component_images(pkg_file_path, output_dir):
    """
    Extract component images from a PLDM FW update package.

    Parses the package header to locate each component image by its
    ComponentLocationOffset and ComponentSize, then writes each image
    to the output directory.

        Parameters:
            pkg_file_path: Path to the PLDM FW update package
            output_dir: Directory to write extracted component images
    """
    with open(pkg_file_path, "rb") as f:
        # PackageHeaderIdentifier (16 bytes)
        if len(f.read(16)) < 16:
            sys.exit("ERROR: File too short to be a valid PLDM package")

        # PackageHeaderFormatRevision (1 byte)
        format_revision = struct.unpack("<B", f.read(1))[0]
        if format_revision not in expected_package_format_versions:
            sys.exit(
                "ERROR: Unsupported package format version %d"
                % format_revision
            )

        # PackageHeaderSize (2 bytes)
        package_header_size = struct.unpack("<H", f.read(2))[0]

        # PackageReleaseDateTime (13 bytes)
        f.read(13)

        # ComponentBitmapBitLength (2 bytes)
        f.read(2)

        # PackageVersionStringType (1 byte)
        f.read(1)
        # PackageVersionStringLength (1 byte) + PackageVersionString
        pkg_ver_str_len = struct.unpack("<B", f.read(1))[0]
        pkg_ver_str = f.read(pkg_ver_str_len).decode("ascii", errors="replace")

        print(
            "Package: version='{}' format_revision={} header_size={} bytes".format(
                pkg_ver_str, format_revision, package_header_size
            )
        )

        # DeviceIDRecordCount (1 byte); skip each record via RecordLength
        device_id_record_count = struct.unpack("<B", f.read(1))[0]
        for _ in range(device_id_record_count):
            record_length = struct.unpack("<H", f.read(2))[0]
            f.read(record_length - 2)

        # DownstreamDeviceIDRecords (format version >= 2)
        if format_revision >= 2:
            downstream_count = struct.unpack("<B", f.read(1))[0]
            for _ in range(downstream_count):
                record_length = struct.unpack("<H", f.read(2))[0]
                f.read(record_length - 2)

        # ComponentImageCount (2 bytes)
        component_image_count = struct.unpack("<H", f.read(2))[0]
        print("Component count: {}".format(component_image_count))

        # Parse each ComponentImageInformation entry
        components = []
        for i in range(component_image_count):
            classification = struct.unpack("<H", f.read(2))[
                0
            ]  # ComponentClassification
            identifier = struct.unpack("<H", f.read(2))[
                0
            ]  # ComponentIdentifier
            f.read(4)  # ComponentComparisonStamp
            f.read(2)  # ComponentOptions
            f.read(2)  # RequestedComponentActivationMethod
            location_offset = struct.unpack("<I", f.read(4))[
                0
            ]  # ComponentLocationOffset
            component_size = struct.unpack("<I", f.read(4))[0]  # ComponentSize
            f.read(1)  # ComponentVersionStringType
            ver_str_len = struct.unpack("<B", f.read(1))[0]
            ver_str = f.read(ver_str_len).decode("ascii", errors="replace")
            if format_revision >= 3:
                opaque_data_len = struct.unpack("<I", f.read(4))[0]
                f.read(opaque_data_len)
            components.append(
                {
                    "index": i,
                    "classification": classification,
                    "identifier": identifier,
                    "location_offset": location_offset,
                    "size": component_size,
                    "version_string": ver_str,
                }
            )

        # Extract each component image to the output directory
        os.makedirs(output_dir, exist_ok=True)
        for comp in components:
            f.seek(comp["location_offset"])
            image_data = f.read(comp["size"])
            # Sanitize version string for use as a filename
            safe_ver = "".join(
                c if c.isalnum() or c in "-_." else "_"
                for c in comp["version_string"]
            ).strip("_") or "component_{:d}".format(comp["index"])
            output_filename = os.path.join(output_dir, safe_ver + ".bin")
            # Avoid overwriting if multiple components share the same version string
            if os.path.exists(output_filename):
                output_filename = os.path.join(
                    output_dir,
                    "{}_cls{:04X}_id{:04X}.bin".format(
                        safe_ver,
                        comp["classification"],
                        comp["identifier"],
                    ),
                )
            with open(output_filename, "wb") as out_file:
                out_file.write(image_data)
            print(
                "  [{}] version='{}' cls=0x{:04X} id=0x{:04X}"
                " size={} bytes -> {}".format(
                    comp["index"],
                    comp["version_string"],
                    comp["classification"],
                    comp["identifier"],
                    comp["size"],
                    output_filename,
                )
            )


def _parse_descriptors(f, descriptor_count):
    """Parse and return a list of descriptor dicts from the current file position."""
    descriptors = []
    for _ in range(descriptor_count):
        desc_type = struct.unpack("<H", f.read(2))[0]
        desc_len = struct.unpack("<H", f.read(2))[0]
        desc_data = f.read(desc_len)
        if desc_type == 0xFFFF:
            desc_name = "Vendor Defined"
        else:
            entry = descriptor_type_name_length.get(desc_type)
            desc_name = entry[0] if entry else "Unknown"
        descriptors.append(
            {
                "type": desc_type,
                "name": desc_name,
                "length": desc_len,
                "data": desc_data,
            }
        )
    return descriptors


def _print_descriptors(descriptors, indent="      "):
    for i, d in enumerate(descriptors):
        print(
            "{}[{}] type=0x{:04X} ({}) len={} data={}".format(
                indent, i, d["type"], d["name"], d["length"], d["data"].hex()
            )
        )


def analyze_package(pkg_file_path):
    """
    Analyze and print the full contents of a PLDM FW update package header.

        Parameters:
            pkg_file_path: Path to the PLDM FW update package
    """
    with open(pkg_file_path, "rb") as f:
        print("=" * 70)
        print("PLDM FW Update Package Analysis")
        print("  File: {}".format(pkg_file_path))
        print("=" * 70)

        # --- Package Header Information ---
        pkg_header_id = f.read(16)
        if len(pkg_header_id) < 16:
            sys.exit("ERROR: File too short to be a valid PLDM package")

        print("\n[Package Header Information]")
        # Format UUID as xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        h = pkg_header_id.hex()
        uuid_str = "{}-{}-{}-{}-{}".format(h[0:8], h[8:12], h[12:16], h[16:20], h[20:32])
        print("  PackageHeaderIdentifier    : {}".format(uuid_str))

        format_revision = struct.unpack("<B", f.read(1))[0]
        if format_revision not in expected_package_format_versions:
            sys.exit(
                "ERROR: Unsupported package format version %d" % format_revision
            )
        print("  PackageHeaderFormatRevision: {} (DSP0267 v1.{}.0)".format(
            format_revision, format_revision - 1
        ))

        package_header_size = struct.unpack("<H", f.read(2))[0]
        print("  PackageHeaderSize          : {} bytes".format(package_header_size))

        # PackageReleaseDateTime: "<hBBBBBBBBHB" = 13 bytes
        dt_raw = f.read(13)
        _utc_off, us0, us1, us2, sec, minute, hour, day, month = \
            struct.unpack_from("<hBBBBBBBB", dt_raw, 0)
        year, _utc_dir = struct.unpack_from("<HB", dt_raw, 10)
        microseconds = us0 | (us1 << 8) | (us2 << 16)
        print(
            "  PackageReleaseDateTime     : "
            "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:06d}".format(
                year, month, day, hour, minute, sec, microseconds
            )
        )

        component_bitmap_bit_length = struct.unpack("<H", f.read(2))[0]
        print("  ComponentBitmapBitLength   : {}".format(component_bitmap_bit_length))

        pkg_ver_str_type = struct.unpack("<B", f.read(1))[0]
        pkg_ver_str_len = struct.unpack("<B", f.read(1))[0]
        pkg_ver_str = f.read(pkg_ver_str_len).decode("ascii", errors="replace")
        print("  PackageVersionString       : '{}' (type={}, len={})".format(
            pkg_ver_str, pkg_ver_str_type, pkg_ver_str_len
        ))

        # --- Firmware Device Identification Area ---
        print("\n[Firmware Device Identification Area]")
        device_id_record_count = struct.unpack("<B", f.read(1))[0]
        print("  DeviceIDRecordCount : {}".format(device_id_record_count))

        for dev_idx in range(device_id_record_count):
            print("\n  [FW Device Record {}]".format(dev_idx))
            record_length = struct.unpack("<H", f.read(2))[0]
            descriptor_count = struct.unpack("<B", f.read(1))[0]
            dev_update_flags = struct.unpack("<I", f.read(4))[0]
            _comp_set_ver_str_type = struct.unpack("<B", f.read(1))[0]
            comp_set_ver_str_len = struct.unpack("<B", f.read(1))[0]
            fw_pkg_data_len = struct.unpack("<H", f.read(2))[0]

            ref_manifest_len = 0
            if format_revision >= 4:
                ref_manifest_len = struct.unpack("<I", f.read(4))[0]

            applicable_bytes = f.read(component_bitmap_bit_length // 8)
            comp_set_ver_str = f.read(comp_set_ver_str_len).decode(
                "ascii", errors="replace"
            )

            applicable = [
                byte_idx * 8 + bit
                for byte_idx, bval in enumerate(applicable_bytes)
                for bit in range(8)
                if bval & (1 << bit)
            ]

            print("    RecordLength             : {}".format(record_length))
            print("    DescriptorCount          : {}".format(descriptor_count))
            print("    DeviceUpdateOptionFlags  : 0x{:08X}".format(dev_update_flags))
            print("    CompImgSetVersionString  : '{}'".format(comp_set_ver_str))
            print("    ApplicableComponents     : {}".format(applicable))
            print("    FWDevicePkgDataLength    : {}".format(fw_pkg_data_len))
            if format_revision >= 4:
                print("    ReferenceManifestLength  : {}".format(ref_manifest_len))

            print("    Descriptors:")
            _print_descriptors(_parse_descriptors(f, descriptor_count))

            if fw_pkg_data_len > 0:
                f.read(fw_pkg_data_len)
            if format_revision >= 4 and ref_manifest_len > 0:
                f.read(ref_manifest_len)

        # --- Downstream Device Identification Area (v2+) ---
        if format_revision >= 2:
            print("\n[Downstream Device Identification Area]")
            downstream_count = struct.unpack("<B", f.read(1))[0]
            print("  DownstreamDeviceIDRecordCount : {}".format(downstream_count))

            for ds_idx in range(downstream_count):
                print("\n  [Downstream Device Record {}]".format(ds_idx))
                record_length = struct.unpack("<H", f.read(2))[0]
                descriptor_count = struct.unpack("<B", f.read(1))[0]
                ds_flags = struct.unpack("<I", f.read(4))[0]
                _ds_min_ver_str_type = struct.unpack("<B", f.read(1))[0]
                ds_min_ver_str_len = struct.unpack("<B", f.read(1))[0]
                ds_pkg_data_len = struct.unpack("<H", f.read(2))[0]

                ds_ref_manifest_len = 0
                if format_revision >= 4:
                    ds_ref_manifest_len = struct.unpack("<I", f.read(4))[0]

                applicable_bytes = f.read(component_bitmap_bit_length // 8)
                ds_min_ver_str = f.read(ds_min_ver_str_len).decode(
                    "ascii", errors="replace"
                )

                applicable = [
                    byte_idx * 8 + bit
                    for byte_idx, bval in enumerate(applicable_bytes)
                    for bit in range(8)
                    if bval & (1 << bit)
                ]

                print("    RecordLength                     : {}".format(record_length))
                print("    DescriptorCount                  : {}".format(descriptor_count))
                print("    DownstreamDeviceUpdateOptionFlags: 0x{:08X}".format(ds_flags))
                print("    SelfContainedActivMinVerString   : '{}'".format(ds_min_ver_str))
                print("    ApplicableComponents             : {}".format(applicable))
                print("    DSDevicePkgDataLength            : {}".format(ds_pkg_data_len))
                if format_revision >= 4:
                    print("    ReferenceManifestLength          : {}".format(ds_ref_manifest_len))

                # ComparisonStamp is written before descriptors when flag bit 0 is set
                if ds_flags & 0x1:
                    cmp_stamp = struct.unpack("<I", f.read(4))[0]
                    print(
                        "    MinVersionComparisonStamp        : 0x{:08X}".format(
                            cmp_stamp
                        )
                    )

                print("    Descriptors:")
                _print_descriptors(_parse_descriptors(f, descriptor_count))

                if ds_pkg_data_len > 0:
                    f.read(ds_pkg_data_len)
                if format_revision >= 4 and ds_ref_manifest_len > 0:
                    f.read(ds_ref_manifest_len)

        # --- Component Image Information Area ---
        print("\n[Component Image Information Area]")
        component_image_count = struct.unpack("<H", f.read(2))[0]
        print("  ComponentImageCount : {}".format(component_image_count))

        for comp_idx in range(component_image_count):
            print("\n  [Component {}]".format(comp_idx))
            classification = struct.unpack("<H", f.read(2))[0]
            identifier = struct.unpack("<H", f.read(2))[0]
            comparison_stamp = struct.unpack("<I", f.read(4))[0]
            comp_options = struct.unpack("<H", f.read(2))[0]
            activation_method = struct.unpack("<H", f.read(2))[0]
            location_offset = struct.unpack("<I", f.read(4))[0]
            component_size = struct.unpack("<I", f.read(4))[0]
            ver_str_type = struct.unpack("<B", f.read(1))[0]
            ver_str_len = struct.unpack("<B", f.read(1))[0]
            ver_str = f.read(ver_str_len).decode("ascii", errors="replace")

            opaque_data_len = 0
            if format_revision >= 3:
                opaque_data_len = struct.unpack("<I", f.read(4))[0]
                if opaque_data_len > 0:
                    f.read(opaque_data_len)

            # Decode ComponentOptions bits
            options_flags = []
            if comp_options & (1 << 0):
                options_flags.append("ForceUpdate")
            if comp_options & (1 << 1):
                options_flags.append("UseComponentCompStamp")
            if comp_options & (1 << 2):
                options_flags.append("DowngradeAllowed")

            # Decode RequestedComponentActivationMethod bits
            activation_flags = []
            activation_names = [
                "AutomaticSelfContained",
                "SelfContainedWithRestrictions",
                "MediumSpecificReset",
                "SystemReboot",
                "DCPowerCycle",
                "ACPowerCycle",
            ]
            for bit, name in enumerate(activation_names):
                if activation_method & (1 << bit):
                    activation_flags.append(name)

            print(
                "    ComponentClassification            : 0x{:04X}".format(
                    classification
                )
            )
            print(
                "    ComponentIdentifier                : 0x{:04X}".format(
                    identifier
                )
            )
            stamp_note = (
                " (default/unused)"
                if comparison_stamp == 0xFFFFFFFF
                else ""
            )
            print(
                "    ComponentComparisonStamp           : 0x{:08X}{}".format(
                    comparison_stamp, stamp_note
                )
            )
            print(
                "    ComponentOptions                   : 0x{:04X} {}".format(
                    comp_options,
                    "[{}]".format(", ".join(options_flags)) if options_flags else "[]",
                )
            )
            print(
                "    RequestedComponentActivationMethod : 0x{:04X} {}".format(
                    activation_method,
                    (
                        "[{}]".format(", ".join(activation_flags))
                        if activation_flags
                        else "[]"
                    ),
                )
            )
            print(
                "    ComponentLocationOffset            : 0x{:08X} ({} bytes from start)".format(
                    location_offset, location_offset
                )
            )
            print(
                "    ComponentSize                      : {} bytes".format(
                    component_size
                )
            )
            print(
                "    ComponentVersionString             : '{}' (type={}, len={})".format(
                    ver_str, ver_str_type, ver_str_len
                )
            )
            if format_revision >= 3:
                print(
                    "    ComponentOpaqueDataLength          : {}".format(
                        opaque_data_len
                    )
                )

        # --- Checksums ---
        print("\n[Checksums]")
        header_end_pos = f.tell()
        stored_header_checksum = struct.unpack("<I", f.read(4))[0]

        f.seek(0)
        computed_header_checksum = binascii.crc32(f.read(header_end_pos)) & 0xFFFFFFFF
        header_ok = (
            "OK"
            if computed_header_checksum == stored_header_checksum
            else "MISMATCH (expected 0x{:08X})".format(computed_header_checksum)
        )
        print(
            "  PackageHeaderChecksum  : 0x{:08X} [{}]".format(
                stored_header_checksum, header_ok
            )
        )

        if format_revision >= 4:
            stored_payload_checksum = struct.unpack("<I", f.read(4))[0]
            payload_data = f.read()
            computed_payload_checksum = binascii.crc32(payload_data) & 0xFFFFFFFF
            payload_ok = (
                "OK"
                if computed_payload_checksum == stored_payload_checksum
                else "MISMATCH (expected 0x{:08X})".format(
                    computed_payload_checksum
                )
            )
            print(
                "  PackagePayloadChecksum : 0x{:08X} [{}]".format(
                    stored_payload_checksum, payload_ok
                )
            )

        # --- Summary ---
        f.seek(0, os.SEEK_END)
        total_file_size = f.tell()
        payload_size = total_file_size - package_header_size
        print("\n[Summary]")
        print("  Total file size : {} bytes".format(total_file_size))
        print("  Header size     : {} bytes".format(package_header_size))
        print("  Payload size    : {} bytes".format(payload_size))
        print("=" * 70)


def main():
    """Create or extract PLDM FW update (DSP0267) packages"""
    parser = argparse.ArgumentParser(
        description="Create or extract PLDM FW update (DSP0267) packages"
    )
    subparsers = parser.add_subparsers(dest="command")
    subparsers.required = True

    # 'create' subcommand (existing functionality)
    create_parser = subparsers.add_parser(
        "create", help="Create a PLDM FW update package"
    )
    create_parser.add_argument(
        "pldmfwuppkgname", help="Name of the PLDM FW update package"
    )
    create_parser.add_argument(
        "metadatafile", help="Path of metadata JSON file"
    )
    create_parser.add_argument(
        "images",
        nargs="+",
        help=(
            "One or more firmware image paths, in the same order as"
            " ComponentImageInformationArea entries"
        ),
    )

    # 'extract' subcommand (new functionality)
    extract_parser = subparsers.add_parser(
        "extract", help="Extract component images from a PLDM FW update package"
    )
    extract_parser.add_argument(
        "pldmfwuppkgname", help="Path to the PLDM FW update package"
    )
    extract_parser.add_argument(
        "outputdir",
        nargs="?",
        default=".",
        help="Output directory for extracted components (default: current directory)",
    )

    # 'analyze' subcommand (new functionality)
    analyze_parser = subparsers.add_parser(
        "analyze", help="Analyze and display PLDM FW update package header contents"
    )
    analyze_parser.add_argument(
        "pldmfwuppkgname", help="Path to the PLDM FW update package"
    )

    args = parser.parse_args()

    if args.command == "create":
        image_files = args.images
        with open(args.metadatafile) as file:
            try:
                metadata = json.load(file)
            except ValueError:
                sys.exit("ERROR: Invalid metadata JSON file")

        # Validate the number of component images
        if len(image_files) != len(metadata["ComponentImageInformationArea"]):
            sys.exit(
                "ERROR: number of images passed != number of entries"
                " in ComponentImageInformationArea"
            )

        try:
            with open(args.pldmfwuppkgname, "w+b") as pldm_fw_up_pkg:
                package_header_format_revision, component_bitmap_bit_length = (
                    write_pkg_header_info(pldm_fw_up_pkg, metadata)
                )
                if (
                    package_header_format_revision
                    not in expected_package_format_versions
                ):
                    sys.exit(
                        "ERROR: Unsupported package format version %d"
                        % package_header_format_revision
                    )
                write_fw_device_identification_area(
                    pldm_fw_up_pkg,
                    metadata,
                    package_header_format_revision,
                    component_bitmap_bit_length,
                )
                if package_header_format_revision >= 2:
                    write_downstream_device_identification_area(
                        pldm_fw_up_pkg,
                        metadata,
                        package_header_format_revision,
                        component_bitmap_bit_length,
                    )
                write_component_image_info_area(
                    pldm_fw_up_pkg,
                    metadata,
                    package_header_format_revision,
                    image_files,
                )
                update_pkg_header_size(
                    pldm_fw_up_pkg, package_header_format_revision
                )
                write_pkg_header_checksum(pldm_fw_up_pkg)
                if package_header_format_revision >= 4:
                    write_pkg_payload_checksum(pldm_fw_up_pkg, image_files)
                append_component_images(pldm_fw_up_pkg, image_files)
                pldm_fw_up_pkg.close()
        except BaseException:
            pldm_fw_up_pkg.close()
            os.remove(args.pldmfwuppkgname)
            raise

    elif args.command == "extract":
        extract_component_images(args.pldmfwuppkgname, args.outputdir)

    elif args.command == "analyze":
        analyze_package(args.pldmfwuppkgname)


if __name__ == "__main__":
    main()
