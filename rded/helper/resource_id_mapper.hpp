/**
 *
 * This file has a hardcoded resource id mapper for mackback machines
 * This would not be used once the PDR is implemented
 *
 */
#include <iostream>
#include <map>
#include <string>
#include <string_view>

std::map<std::string_view, uint32_t> resourceIdMap;
int createResourceIdMapping()
{
    resourceIdMap.insert(std::make_pair("/redfish/v1/Chassis", 0x00000000));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/Tray", 0x00010000));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_0", 0x00010001));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_1", 0x00010002));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_2", 0x00010003));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_3", 0x00010004));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_4", 0x00010005));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_5", 0x00010006));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_6", 0x00010007));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_7", 0x00010008));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_8", 0x00010009));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_9", 0x0001000a));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_10", 0x0001000b));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_11", 0x0001000c));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_12", 0x0001000d));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_13", 0x0001000e));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_14", 0x0001000f));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_15", 0x00010010));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_16", 0x00010011));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_17", 0x00010012));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_18", 0x00010013));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_19", 0x00010014));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_20", 0x00010015));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_21", 0x00010016));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_22", 0x00010017));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_23", 0x00010018));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/Tray/Sensors", 0x00020000));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_0/Sensors", 0x00020001));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_1/Sensors", 0x00020002));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_2/Sensors", 0x00020003));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_3/Sensors", 0x00020004));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_4/Sensors", 0x00020005));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_5/Sensors", 0x00020006));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_6/Sensors", 0x00020007));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_7/Sensors", 0x00020008));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_8/Sensors", 0x00020009));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_9/Sensors", 0x0002000a));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_10/Sensors", 0x0002000b));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_11/Sensors", 0x0002000c));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_12/Sensors", 0x0002000d));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_13/Sensors", 0x0002000e));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_14/Sensors", 0x0002000f));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_15/Sensors", 0x00020010));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_16/Sensors", 0x00020011));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_17/Sensors", 0x00020012));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_18/Sensors", 0x00020013));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_19/Sensors", 0x00020014));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_20/Sensors", 0x00020015));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_21/Sensors", 0x00020016));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_22/Sensors", 0x00020017));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_23/Sensors", 0x00020018));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p12v_scaled", 0x00030000));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p5v1_scaled", 0x00030001));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p5v2_scaled", 0x00030002));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p3v3_mr_scaled", 0x00030003));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p1v8_scaled", 0x00030004));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p0v86_scaled", 0x00030005));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/vol_p1v45_scaled", 0x00030006));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/temp_max31725", 0x00030007));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_temp", 0x00030008));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/Tray/Sensors/hsc_vin", 0x00030009));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_vout", 0x0003000a));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/Tray/Sensors/hsc_pin", 0x0003000b));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_iout", 0x0003000c));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_peak_iout", 0x0003000d));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_peak_vin", 0x0003000e));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_peak_temp", 0x0003000f));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/hsc_peak_pin", 0x00030010));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/brick_p12v_temp", 0x00030011));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/brick_p12v_vin", 0x00030012));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/brick_p12v_vout", 0x00030013));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/brick_p12v_iout", 0x00030014));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/brick_p12v_pout", 0x00030015));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_vin", 0x00030016));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_vout", 0x00030017));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_iin", 0x00030018));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_iout", 0x00030019));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_pin", 0x0003001a));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_pout", 0x0003001b));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_1_temp", 0x0003001c));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_vin", 0x0003001d));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_vout", 0x0003001e));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_iin", 0x0003001f));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_iout", 0x00030020));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_pin", 0x00030021));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_pout", 0x00030022));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p5v_2_temp", 0x00030023));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_vin", 0x00030024));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_vout", 0x00030025));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_iin", 0x00030026));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_iout", 0x00030027));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_pin", 0x00030028));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_pout", 0x00030029));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/p0v86_temp", 0x0003002a));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/fan0_tach", 0x0003002b));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/fan1_tach", 0x0003002c));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/fan0_duty", 0x0003002d));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/fan1_duty", 0x0003002e));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/ioc_temp", 0x0003002f));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_0/Sensors/Drive_0t", 0x00030030));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_1/Sensors/Drive_0t", 0x00030031));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_2/Sensors/Drive_0t", 0x00030032));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_3/Sensors/Drive_0t", 0x00030033));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_4/Sensors/Drive_0t", 0x00030034));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_5/Sensors/Drive_0t", 0x00030035));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_6/Sensors/Drive_0t", 0x00030036));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_7/Sensors/Drive_0t", 0x00030037));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_8/Sensors/Drive_0t", 0x00030038));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_9/Sensors/Drive_0t", 0x00030039));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_10/Sensors/Drive_0t", 0x0003003a));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_11/Sensors/Drive_0t", 0x0003003b));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_12/Sensors/Drive_0t", 0x0003003c));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_13/Sensors/Drive_0t", 0x0003003d));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_14/Sensors/Drive_0t", 0x0003003e));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_15/Sensors/Drive_0t", 0x0003003f));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_16/Sensors/Drive_0t", 0x00030040));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_17/Sensors/Drive_0t", 0x00030041));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_18/Sensors/Drive_0t", 0x00030042));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_19/Sensors/Drive_0t", 0x00030043));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_20/Sensors/Drive_0t", 0x00030044));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_21/Sensors/Drive_0t", 0x00030045));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_22/Sensors/Drive_0t", 0x00030046));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_23/Sensors/Drive_0t", 0x00030047));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/Tray/Sensors/host_fan0_pwm", 0x00030048));
    resourceIdMap.insert(std::make_pair("/redfish/v1/Managers", 0x00040000));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Managers/mbmc", 0x00050000));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_0/Drives", 0x00060001));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_1/Drives", 0x00060002));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_2/Drives", 0x00060003));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_3/Drives", 0x00060004));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_4/Drives", 0x00060005));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_5/Drives", 0x00060006));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_6/Drives", 0x00060007));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_7/Drives", 0x00060008));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_8/Drives", 0x00060009));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_9/Drives", 0x0006000a));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_10/Drives", 0x0006000b));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_11/Drives", 0x0006000c));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_12/Drives", 0x0006000d));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_13/Drives", 0x0006000e));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_14/Drives", 0x0006000f));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_15/Drives", 0x00060010));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_16/Drives", 0x00060011));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_17/Drives", 0x00060012));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_18/Drives", 0x00060013));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_19/Drives", 0x00060014));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_20/Drives", 0x00060015));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_21/Drives", 0x00060016));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_22/Drives", 0x00060017));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_23/Drives", 0x00060018));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_0/Drives/SATA_0", 0x00070000));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_1/Drives/SATA_1", 0x00070001));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_2/Drives/SATA_2", 0x00070002));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_3/Drives/SATA_3", 0x00070003));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_4/Drives/SATA_4", 0x00070004));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_5/Drives/SATA_5", 0x00070005));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_6/Drives/SATA_6", 0x00070006));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_7/Drives/SATA_7", 0x00070007));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_8/Drives/SATA_8", 0x00070008));
    resourceIdMap.insert(
        std::make_pair("/redfish/v1/Chassis/SATA_9/Drives/SATA_9", 0x00070009));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_10/Drives/SATA_10", 0x0007000a));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_11/Drives/SATA_11", 0x0007000b));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_12/Drives/SATA_12", 0x0007000c));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_13/Drives/SATA_13", 0x0007000d));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_14/Drives/SATA_14", 0x0007000e));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_15/Drives/SATA_15", 0x0007000f));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_16/Drives/SATA_16", 0x00070010));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_17/Drives/SATA_17", 0x00070011));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_18/Drives/SATA_18", 0x00070012));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_19/Drives/SATA_19", 0x00070013));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_20/Drives/SATA_20", 0x00070014));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_21/Drives/SATA_21", 0x00070015));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_22/Drives/SATA_22", 0x00070016));
    resourceIdMap.insert(std::make_pair(
        "/redfish/v1/Chassis/SATA_23/Drives/SATA_23", 0x00070017));
    return 0;
}

int getResourceIdForUri(std::string_view uri, uint32_t* resourceId)
{
    // std::cerr << "URI: " << uri << "\n";
    auto it = resourceIdMap.find(uri);
    if (it != resourceIdMap.end())
    {
        *resourceId = it->second;
        std::cerr << "Resource id found: " << std::to_string(it->second)
                  << "\n";
        return 0;
    }
    std::cerr << "Resource id not found \n";
    return 1;
}