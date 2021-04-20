#!/usr/bin/python3
import struct
from datetime import datetime
import json
import sys
import argparse
from bitarray import bitarray
from bitarray.util import ba2int
import os
import binascii


stringTypes = dict([
    ("Unknown", 0),
    ("ASCII", 1),
    ("UTF8", 2),
    ("UTF16", 3),
    ("UTF16LE", 4),
    ("UTF16BE", 5)])


def checkStringLength(stringLength):
    if stringLength > 255:
        sys.exit("ERROR: Max permitted string length is 255")


def writePackageReleaseDateTime(pldmFwUpPkg):
    now = datetime.now()
    time = now.time()
    date = now.date()
    usbytes = time.microsecond.to_bytes(3, byteorder='little')
    pldmFwUpPkg.write(
        struct.pack(
            '<hBBBBBBBBHB',
            0,
            usbytes[0],
            usbytes[1],
            usbytes[2],
            time.second,
            time.minute,
            time.hour,
            date.day,
            date.month,
            date.year,
            0))


def writePackageVersionString(pldmFwUpPkg, metadata):

    packageVersionStringType = stringTypes["ASCII"]
    packageVersionString = metadata["PackageHeaderInformation"]["PackageVersionString"]
    packageVersionStringLength = len(packageVersionString)
    checkStringLength(packageVersionStringLength)
    formatString = '<BB' + str(packageVersionStringLength) + 's'
    pldmFwUpPkg.write(
        struct.pack(
            formatString,
            packageVersionStringType,
            packageVersionStringLength,
            packageVersionString.encode('ascii')))


def writeComponentBitmapLength(pldmFwUpPkg, metadata):
    components = metadata["ComponentImageInformationArea"]
    numComponents = len(components)
    if numComponents > 32:
        sys.exit("ERROR: only upto 32 components supported at the moment")
    componentBitmapByteLength = int(numComponents / 8)
    if numComponents % 8:
        componentBitmapByteLength += 1
    componentBitmapBitLength = componentBitmapByteLength * 8
    pldmFwUpPkg.write(struct.pack('<H', int(componentBitmapBitLength)))
    return components


def writePackageHeaderInformation(pldmFwUpPkg, metadata):
    pldmFWUpdatePkgUUID = "1244D2648D7D4718A030FC8A56587D5A"
    packageHeaderIdentifier = bytearray.fromhex(pldmFWUpdatePkgUUID)
    pldmFwUpPkg.write(packageHeaderIdentifier)

    packageHeaderFormatRevision = 2
    # Size will be computed and updated subsequently
    packageHeaderSize = 0
    pldmFwUpPkg.write(
        struct.pack(
            '<BH',
            packageHeaderFormatRevision,
            packageHeaderSize))

    writePackageReleaseDateTime(pldmFwUpPkg)

    components = writeComponentBitmapLength(
        pldmFwUpPkg, metadata)

    writePackageVersionString(pldmFwUpPkg, metadata)

    return components


def getApplicableComponents(device, components):
    applicableComponentsList = device["ApplicableComponents"]

    applicableComponents = bitarray(len(components), endian='little')
    applicableComponents.setall(0)
    for component in components:
        if component["ComponentIdentifier"] in applicableComponentsList:
            applicableComponents[components.index(component)] = 1
    return applicableComponents


def writeFirmwareDeviceIdentificationArea(
        pldmFwUpPkg, metadata, components):
    devices = metadata["FirmwareDeviceIdentificationArea"]
    deviceIDRecordCount = len(devices)
    if deviceIDRecordCount > 255:
        sys.exit(
            "ERROR: there can be only upto 255 entries in the FirmwareDeviceIdentificationArea section")
    pldmFwUpPkg.write(struct.pack('<B', deviceIDRecordCount))
    for device in devices:
        recordLength = 2
        initialDescriptor = device["InitialDescriptor"]
        descriptorCount = 1
        recordLength += 1
        deviceUpdateOptionFlags = bitarray(32, endian='little')
        deviceUpdateOptionFlags.setall(0)
        supportedDeviceUpdateOptionFlags = [0]
        for option in device["DeviceUpdateOptionFlags"]:
            if option not in supportedDeviceUpdateOptionFlags:
                sys.exit("ERROR: unsupported DeviceUpdateOptionFlag entry")
            deviceUpdateOptionFlags[option] = 1
        recordLength += 4
        componentImageSetVersionStringType = stringTypes["ASCII"]
        recordLength += 1
        componentImageSetVersionString = device["ComponentImageSetVersionString"]
        recordLength += len(componentImageSetVersionString)
        componentImageSetVersionStringLength = len(
            componentImageSetVersionString)
        checkStringLength(componentImageSetVersionStringLength)
        recordLength += 1
        firmwareDevicePackageDataLength = 0
        recordLength += 2
        applicableComponents = getApplicableComponents(
            device, components)
        recordLength += 4
        initialDescriptorType = initialDescriptor["InitialDescriptorType"]
        if initialDescriptorType != 2:
            sys.exit("ERROR: Currently supported descriptor id is UUID alone")
        recordLength += 2
        initialDescriptorLength = 16
        recordLength += 2
        initialDescriptorData = initialDescriptor["InitialDescriptorData"]
        recordLength += 16
        formatString = '<HBIBBHI' + \
            str(componentImageSetVersionStringLength) + 'sHH'
        pldmFwUpPkg.write(
            struct.pack(
                formatString,
                recordLength,
                descriptorCount,
                ba2int(deviceUpdateOptionFlags),
                componentImageSetVersionStringType,
                componentImageSetVersionStringLength,
                firmwareDevicePackageDataLength,
                ba2int(applicableComponents),
                componentImageSetVersionString.encode('ascii'),
                initialDescriptorType,
                initialDescriptorLength))
        pldmFwUpPkg.write(bytearray.fromhex(initialDescriptorData))


def writeComponentImageInformationArea(pldmFwUpPkg, components, imageFiles):
    componentImageCount = len(components)
    pldmFwUpPkg.write(struct.pack('<H', componentImageCount))
    componentLocationOffsets = []
    for component in components:
        # ComponentLocationOffset starts at the 12th byte in the individual
        # component image area
        componentLocationOffsets.append(pldmFwUpPkg.tell() + 12)
        componentClassification = component["ComponentClassification"]
        if componentClassification < 0 or componentClassification > 0xFFFF:
            sys.exit(
                "ERROR: ComponentClassification should be [0x0000 - 0xFFFF]")
        componentIdentifier = component["ComponentIdentifier"]
        if componentIdentifier < 0 or componentIdentifier > 0xFFFF:
            sys.exit(
                "ERROR: ComponentIdentifier should be [0x0000 - 0xFFFF]")
        componentComparisonStamp = 0xFFFFFFFF
        componentOptions = bitarray(16, endian='little')
        componentOptions.setall(0)
        supportedComponentOptions = [0]
        for option in component["ComponentOptions"]:
            if option not in supportedComponentOptions:
                sys.exit(
                    "ERROR: unsupported ComponentOption in ComponentImageInformationArea section")
            componentOptions[option] = 1
        requestedComponentActivationMethod = bitarray(16, endian='little')
        requestedComponentActivationMethod.setall(0)
        supportedRequestedComponentActivationMethod = [0, 1, 2, 3, 4, 5]
        for option in component["RequestedComponentActivationMethod"]:
            if option not in supportedRequestedComponentActivationMethod:
                sys.exit(
                    "ERROR: unsupported RequestedComponentActivationMethod entry")
            requestedComponentActivationMethod[option] = 1
        componentLocationOffset = 0
        componentSize = 0
        componentVersionStringType = stringTypes["ASCII"]
        componentVersionString = component["ComponentVersionString"]
        componentVersionStringLength = len(componentVersionString)
        checkStringLength(componentVersionStringLength)
        formatStr = '<HHIHHIIBB' + str(componentVersionStringLength) + 's'
        pldmFwUpPkg.write(
            struct.pack(
                formatStr,
                componentClassification,
                componentIdentifier,
                componentComparisonStamp,
                ba2int(componentOptions),
                ba2int(requestedComponentActivationMethod),
                componentLocationOffset,
                componentSize,
                componentVersionStringType,
                componentVersionStringLength,
                componentVersionString.encode('ascii')))

    # Update ComponentLocationOffset and ComponentSize
    index = 0
    startOffset = pldmFwUpPkg.tell()
    for offset in componentLocationOffsets:
        fileSize = os.stat(imageFiles[index]).st_size
        pldmFwUpPkg.seek(offset)
        pldmFwUpPkg.write(
            struct.pack(
                '<II', startOffset, fileSize))
        startOffset += fileSize
        index += 1
    pldmFwUpPkg.seek(0, os.SEEK_END)


def writePackageHeaderChecksum(pldmFwUpPkg):
    pldmFwUpPkg.seek(0)
    packageHeaderChecksum = binascii.crc32(pldmFwUpPkg.read())
    pldmFwUpPkg.seek(0, os.SEEK_END)
    pldmFwUpPkg.write(struct.pack('<I', packageHeaderChecksum))


def updatePackageHeaderSize(pldmFwUpPkg, metadata):
    fileSize = startOffset = pldmFwUpPkg.tell()
    # seek past PackageHeaderIdentifier and PackageHeaderFormatRevision
    pldmFwUpPkg.seek(17)
    pldmFwUpPkg.write(struct.pack('<I', fileSize))
    pldmFwUpPkg.seek(0, os.SEEK_END)


def appendComponentImages(pldmFwUpPkg, imageFiles):
    for image in imageFiles:
        with open(image, 'rb') as f:
            for line in f:
                pldmFwUpPkg.write(line)


def main():
    """Create PLDM FW update (DSP0267) package from a metadata JSON file file"""

    # TODO parse input args
    parser = argparse.ArgumentParser()
    parser.add_argument("metadataFile",
                        help="Path of metadata JSON file")
    parser.add_argument(
        "images", nargs='+',
        help="one or more firmware image paths, in the SAME ORDER as ComponentImageInformationArea entries")
    args = parser.parse_args()
    imageFiles = args.images
    with open(args.metadataFile) as f:
        try:
            metadata = json.load(f)
        except BaseException:
            sys.exit("ERROR: Invalid metadata JSON file")
    if len(imageFiles) != len(metadata["ComponentImageInformationArea"]):
        sys.exit(
            "ERROR: number of images passed != number of entries in ComponentImageInformationArea")

    # TODO validate input JSON via a schema?

    pldmFwUpPkg = open('pldm-fwup-pkg.bin', 'w+b')

    try:
        components = writePackageHeaderInformation(
            pldmFwUpPkg, metadata)
        writeFirmwareDeviceIdentificationArea(
            pldmFwUpPkg, metadata, components)
        writeComponentImageInformationArea(pldmFwUpPkg, components, imageFiles)
        updatePackageHeaderSize(pldmFwUpPkg, metadata)
        writePackageHeaderChecksum(pldmFwUpPkg)
        appendComponentImages(pldmFwUpPkg, imageFiles)
    except BaseException:
        os.remove('pldm-fwup-pkg.bin')
        raise

    pldmFwUpPkg.close()


if __name__ == "__main__":
    main()
