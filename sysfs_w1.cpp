#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include "sysfs_w1.h"

bool operator== (const TSysfsOnewireDevice & first, const TSysfsOnewireDevice & second)
{
    return first.DeviceName == second.DeviceName;
}


TSysfsOnewireDevice::TSysfsOnewireDevice(const string& device_name)
    : DeviceName(device_name)
{
    //FIXME: fill family number
    Family = TOnewireFamilyType::ProgResThermometer;
    DeviceId = DeviceName;
    DeviceDir = SysfsOnewireDevicesPath + DeviceName;
    LastHourErrors = std::vector<int>(60, 0); //Default - any minute has null error
    LastReadTimeMinute = 0; //default for non owerflow
}

int TSysfsOnewireDevice::GetLastHourErrorCount() const{
    int errors = 0;
    for(int minuteError : LastHourErrors){
      errors+= minuteError;
    }
    return errors;
}

TMaybe<float> TSysfsOnewireDevice::ReadTemperature()
{
    std::string data;
    bool bFoundCrcOk=false;

    static const std::string tag("t=");

    std::ifstream file;
    std::string fileName=DeviceDir +"/w1_slave";
    file.open(fileName.c_str());
    if (file.is_open()) {
        std::string sLine;
        while (file.good()) {
            getline(file, sLine);
            size_t tpos;
            if (sLine.find("crc=")!=std::string::npos) {
                if (sLine.find("YES")!=std::string::npos) {
                    bFoundCrcOk=true;
                }
            } else if ((tpos=sLine.find(tag))!=std::string::npos) {
                data = sLine.substr(tpos+tag.length());
            }
        }
        file.close();
    }

    int current_minute = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count()%60;
    if(LastReadTimeMinute!=current_minute){
      LastReadTimeMinute = current_minute;
      LastHourErrors[current_minute] = 0;
    }

    if (bFoundCrcOk) {
        int data_int = std::stoi(data);

        if (data_int == 85000) {
            // wrong read
            LastHourErrors[current_minute]++;
            return NotDefinedMaybe;
        }

        if (data_int == 127937) {
            // returned max possible temp, probably an error
            // (it happens for chineese clones)
            LastHourErrors[current_minute]++;
            return NotDefinedMaybe;
        }


        return (float) data_int/1000.0f; // Temperature given by kernel is in thousandths of degrees
    } else {
      LastHourErrors[current_minute]++;
    }

    return NotDefinedMaybe;
}

void TSysfsOnewireManager::RescanBus()
{
}
