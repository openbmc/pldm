import argparse
import binascii
import enum
import json
import struct


class StringType(enum.Enum):
    Unknown = 0
    ASCII = 1
    UTF8 = 2
    UTF16 = 3
    UTF16LE = 4
    UTF16BE = 5
    

string_type_mapping = {1: "ASCII", 2: "UTF8", 3: "UTF16", 4: "UTF16LE", 5: "UTF16BE"}

ComponentBitmapBitLength_g = 1

descriptor_type_name_length = {
    0x0000: ["PCI Vendor ID", 2],
    0x0001: ["IANA Enterprise ID", 4],
    0x0002: ["UUID", 16],
    0x0003: ["PnP Vendor ID", 3],
    0x0004: ["ACPI Vendor ID", 4],
    0x0100: ["PCI Device ID", 2],
    0x0101: ["PCI Subsystem Vendor ID", 2],
    0x0102: ["PCI Subsystem ID", 2],
    0x0103: ["PCI Revision ID", 1],
    0x0104: ["PnP Product Identifier", 4],
    0x0105: ["ACPI Product Identifier", 4],
}
def read_component_image_information_area(pldm_fw_up_pkg, metadata):
    ComponentImageCount = struct.unpack('H', pldm_fw_up_pkg.read(2))[0]
    metadata["ComponentImageInformationArea"]["ComponentImageCount"] = ComponentImageCount
    
    for i in range(ComponentImageCount):
        component_key = "Component_" + str(i)
        
        if component_key not in metadata["ComponentImageInformationArea"]:
            metadata["ComponentImageInformationArea"][component_key] = {}
            
        ComponentClassification = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentClassification"] = format(ComponentClassification, '04x')
        
        ComponentIdentifier = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentIdentifier"] = format(ComponentIdentifier, '04x')
        
        ComponentComparisonStamp = struct.unpack('<I', pldm_fw_up_pkg.read(4))[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentComparisonStamp"] = format(ComponentComparisonStamp, '08x')
        
        ComponentOptions = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        integer_value = 0
        for k in range(16):
            bit = (ComponentOptions >> k) & 1  # Extract the i-th bit
            integer_value |= (bit << k)  # Set the i-th bit in the integer value
            
        metadata["ComponentImageInformationArea"][component_key]["ComponentOptions"] = format(integer_value, '2b')
        
        RequestedComponentActivationMethod  = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        integer_value = 0
        for k in range(16):
            bit = (RequestedComponentActivationMethod >> k) & 1  # Extract the i-th bit
            integer_value |= (bit << k)  # Set the i-th bit in the integer value
            
        metadata["ComponentImageInformationArea"][component_key]["RequestedComponentActivationMethod"] = format(integer_value, '6b')
        
        ComponentLocationOffset = struct.unpack('<I', pldm_fw_up_pkg.read(4))[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentLocationOffset"] = ComponentLocationOffset
        
        ComponentSize = struct.unpack('<I', pldm_fw_up_pkg.read(4))[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentSize"] = ComponentSize
        
        ComponentVersionStringType = pldm_fw_up_pkg.read(1)[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentVersionStringType"] = string_type_mapping[ComponentVersionStringType]
        
        ComponentVersionStringLength = pldm_fw_up_pkg.read(1)[0]
        metadata["ComponentImageInformationArea"][component_key]["ComponentVersionStringLength"] = ComponentVersionStringLength
        
        ComponentVersionString = pldm_fw_up_pkg.read(ComponentVersionStringLength).decode('ascii')
        metadata["ComponentImageInformationArea"][component_key]["ComponentVersionString"] = ComponentVersionString

def read_firmware_device_identification_area(pldm_fw_up_pkg, metadata):
    DeviceIDRecordCount = struct.unpack('B', pldm_fw_up_pkg.read(1))[0]
    metadata["FirmwareDeviceIdentificationArea"]["DeviceIDRecordCount"] = DeviceIDRecordCount
    
    for i in range(DeviceIDRecordCount):
        RecordLength = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        record_key = "Record_" + str(i)
        metadata["FirmwareDeviceIdentificationArea"][record_key] = {
            "RecordLength": format(RecordLength, '04x')
        }
        
        DescriptorCount = pldm_fw_up_pkg.read(1)[0]
        metadata["FirmwareDeviceIdentificationArea"][record_key]["DescriptorCount"] = DescriptorCount
        
        DeviceUpdateOptionFlags  = pldm_fw_up_pkg.read(4)[0]
        
        integer_value = 0
        for k in range(32):
            bit = (DeviceUpdateOptionFlags >> k) & 1  # Extract the i-th bit
            integer_value |= (bit << k)  # Set the i-th bit in the integer value
        
        metadata["FirmwareDeviceIdentificationArea"][record_key]["DeviceUpdateOptionFlags"] = format(integer_value, '08b')
        ComponentImageSetVersionStringType = pldm_fw_up_pkg.read(1)[0]
        #metadata["FirmwareDeviceIdentificationArea"][record_key]["ComponentImageSetVersionStringType"] = string_type_mapping[ComponentImageSetVersionStringType]
        ComponentImageSetVersionStringLength = struct.unpack('<B', pldm_fw_up_pkg.read(1))[0]
        metadata["FirmwareDeviceIdentificationArea"][record_key]["ComponentImageSetVersionStringLength"] = ComponentImageSetVersionStringLength
        
        FirmwareDevicePackageDataLength =  struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        metadata["FirmwareDeviceIdentificationArea"][record_key]["FirmwareDevicePackageDataLength"] = format(FirmwareDevicePackageDataLength, '04x')
         
        global ComponentBitmapBitLength_g
        ApplicableComponents = pldm_fw_up_pkg.read(1)[0]
        metadata["FirmwareDeviceIdentificationArea"][record_key]["ApplicableComponents"] = format(ApplicableComponents, '08b')
        
        ComponentImageSetVersionString = pldm_fw_up_pkg.read(ComponentImageSetVersionStringLength).decode('ascii')
        metadata["FirmwareDeviceIdentificationArea"][record_key]["ComponentImageSetVersionString"] = ComponentImageSetVersionString
        
        InitialDescriptorType = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        Descriptor_key = "Descriptor_" + str(0)
        
        if Descriptor_key not in metadata["FirmwareDeviceIdentificationArea"][record_key]:
            metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key] = {}
    
        metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key]["InitialDescriptorType"] = InitialDescriptorType
        
        InitialDescriptorLength  = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
        metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key]["InitialDescriptorLength"] = InitialDescriptorLength
        
        InitialDescriptorData = binascii.hexlify(pldm_fw_up_pkg.read(InitialDescriptorLength)).decode()
        metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key]["InitialDescriptorData"] = InitialDescriptorData
        
        
        for a in range(1, DescriptorCount):
            Descriptor_key = "Descriptor_" + str(a)
        
            if Descriptor_key not in metadata["FirmwareDeviceIdentificationArea"][record_key]:
                metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key] = {}
                
            AdditionalDescriptorType = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
            metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key]["AdditionalDescriptorType"] = AdditionalDescriptorType
            
            AdditionalDescriptorLength  = struct.unpack('<H', pldm_fw_up_pkg.read(2))[0]
            metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key]["AdditionalDescriptorLength"] = AdditionalDescriptorLength
        
            AdditionalDescriptorData = binascii.hexlify(pldm_fw_up_pkg.read(AdditionalDescriptorLength)).decode()
            metadata["FirmwareDeviceIdentificationArea"][record_key][Descriptor_key]["AdditionalDescriptorData"] = AdditionalDescriptorData
        


def read_pkg_header_info(pldm_fw_up_pkg, metadata):
    package_header_identifier = binascii.hexlify(pldm_fw_up_pkg.read(16)).decode()
    metadata["PackageHeaderInformation"]["PackageHeaderIdentifier"] = package_header_identifier

    package_header_format_revision, package_header_size = struct.unpack("<BH", pldm_fw_up_pkg.read(3))
    metadata["PackageHeaderInformation"]["PackageHeaderFormatVersion"] = package_header_format_revision
    metadata["PackageHeaderInformation"]["PackageHeaderSize"] = format(package_header_size, '04x')
    
    # Read the binary datetime from the pldm_fw_up_pkg file
    binary_datetime = pldm_fw_up_pkg.read(13)

    # Parse the binary datetime
    year = struct.unpack("<H", binary_datetime[10:12])[0]
    month = binary_datetime[9]
    day = binary_datetime[8]
    hour = binary_datetime[7]
    minute = binary_datetime[6]
    second = binary_datetime[5]

    # Construct the release date and time string
    release_date_time_str = f"{day:02d}/{month:02d}/{year:04d} {hour:02d}:{minute:02d}:{second:02d}"

    # Assign the parsed release date and time to the metadata dictionary
    metadata["PackageHeaderInformation"]["PackageReleaseDateTime"] = release_date_time_str

    # Assign the parsed release date and time to the metadata dictionary
    metadata["PackageHeaderInformation"]["PackageReleaseDateTime"] = release_date_time_str
    
    ComponentBitmapBitLength = struct.unpack("<H", pldm_fw_up_pkg.read(2))[0]
    metadata["PackageHeaderInformation"]["ComponentBitmapBitLength"] = format(ComponentBitmapBitLength, '04x')
    global ComponentBitmapBitLength_g
    ComponentBitmapBitLength_g = ComponentBitmapBitLength
    
    string_type, string_length = struct.unpack("<BB", pldm_fw_up_pkg.read(2))
    metadata["PackageHeaderInformation"]["PackageVersionStringType"] = string_type_mapping[string_type]
    metadata["PackageHeaderInformation"]["PackageVersionStringLength"] = string_length
    
    package_version_string = pldm_fw_up_pkg.read(string_length).decode('ascii')
    metadata["PackageHeaderInformation"]["PackageVersionString"] = package_version_string

def parse_pldm_fw_up_pkg(pldm_fw_up_pkg_path):
    metadata = {
        "PackageHeaderInformation": {},
        "FirmwareDeviceIdentificationArea": {},
        "ComponentImageInformationArea": {},
        "PackageHeaderChecksum": {}
    }

    with open(pldm_fw_up_pkg_path, "rb") as pldm_fw_up_pkg:
        read_pkg_header_info(pldm_fw_up_pkg, metadata)
        read_firmware_device_identification_area(pldm_fw_up_pkg, metadata)
        read_component_image_information_area(pldm_fw_up_pkg, metadata)
        PackageHeaderChecksum = struct.unpack("<I", pldm_fw_up_pkg.read(4))[0]
        metadata["PackageHeaderChecksum"] = format(PackageHeaderChecksum, '08x')

    return metadata


def main():
    parser = argparse.ArgumentParser(description="PLDM Firmware Update Package Parser")
    parser.add_argument("pldm_fw_up_pkg", type=str, help="Path to the PLDM firmware update package")
    parser.add_argument("output_json", type=str, help="Path to the output JSON file")
    args = parser.parse_args()

    pldm_fw_up_pkg_path = args.pldm_fw_up_pkg
    output_json_path = args.output_json

    metadata = parse_pldm_fw_up_pkg(pldm_fw_up_pkg_path)

    with open(output_json_path, "w") as output_file:
        json.dump(metadata, output_file, indent=4)

    print(f"JSON file successfully generated: {output_json_path}")


if __name__ == "__main__":
    main()


