#define _CRT_SECURE_NO_WARNINGS

#include <atomic>
#include <ctime>
#include <exception>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "../src/BTSerialPortBinding.h"
#include "../src/BluetoothException.h"
#include "../src/DeviceINQ.h"
#include "../src/Enums.h"

using namespace std;

string formatDate(const char* format, time_t time) {
    if (time <= 0) return "--";

    char buffer[256] = {0};
    tm* timeinfo = localtime(&time);

    if (timeinfo) strftime(buffer, sizeof(buffer), format, timeinfo);

    return buffer;
}

thread printLoading(const string& message, atomic<bool>& done) {
    cout << message;
    auto logThread = thread([&]() {
        while (!done) {
            cout << "." << flush;
            this_thread::sleep_for(chrono::seconds(1));
        }
    });
    return move(logThread);
}

int main() {
    atomic<bool> done{false};
    device selectedDevice;
    int channelID;
    int choice;
    int i{0};
    string profileIds[] = {"HANDSFREE_PROFILE_ID", "GENERIC_AUDIO_PROFILE_ID"};
    thread logThread;
    unique_ptr<BTSerialPortBinding> binding;
    unique_ptr<DeviceINQ> inq;
    vector<device> devices;
    vector<int> selectedChannels;
    vector<vector<int>> channels;

    logThread = printLoading("Inquiring Bluetooth devices", done);

    try {
        inq = unique_ptr<DeviceINQ>(DeviceINQ::Create());
        devices = inq->Inquire();
    } catch (const BluetoothException& e) {
        cout << e.what() << endl;
        return 1;
    }

    done = true;
    logThread.join();
    cout << endl;

    for (auto& d : devices) {
        d.name = d.name.empty() ? "N/A" : d.name;
        cout << "[" << i++ << "]\tname: " << d.name << endl;
        cout << "\taddress: " << d.address << endl;
        cout << "\tclass: " << GetDeviceClassString(d.deviceClass) << endl;
        cout << "\tmajor class: " << GetDeviceClassString(d.majorDeviceClass)
             << endl;
        cout << "\tservice class: " << GetServiceClassString(d.serviceClass)
             << endl;
        cout << "\tlast seen: " << formatDate("%c", d.lastSeen) << endl;
        cout << "\tlast used: " << formatDate("%c", d.lastUsed) << endl;

        channels.push_back(vector<int>());
        for (const auto& profileId : profileIds) {
            done = false;
            logThread = printLoading("Searching for " + profileId, done);
            try {
                channelID = inq->SdpSearch(d.address, profileId);
            } catch (const BluetoothException& e) {
            }
            done = true;
            logThread.join();
            cout << "\033[2K\r";
            if (channelID > 0) {
                cout << "\t" << profileId.substr(0, profileId.size() - 11)
                     << " Channel ID: " << channelID << endl;
            }

            channels.back().push_back(channelID);
        }
        cout << endl;
    }

    cout << endl << "done, found " << devices.size() << " device(s)" << endl;

    cout << endl << "Select a device to connect to: ";
    cin >> choice;
    if (choice < 0 || choice >= devices.size()) {
        cout << "Invalid choice" << endl;
        return 1;
    }
    selectedDevice = devices[choice];
    selectedChannels = channels[choice];

    try {
        binding = unique_ptr<BTSerialPortBinding>(BTSerialPortBinding::Create(
            selectedDevice.address, selectedChannels[0]));
        binding->Connect();
    } catch (const BluetoothException& e) {
        cout << e.what() << endl;
        return 1;
    }
    cout << "Connected to " << selectedDevice.name << " ["
         << selectedDevice.address << "]" << endl;

    string line;
    getline(cin, line);

    while (true) {
        cout << "\nEnter q to quit> ";
        getline(cin, line);
        if (line == "q" || line == "Q") {
            binding->Close();
            break;
        }
    }

    cout << "done" << endl;

    return 0;
}
