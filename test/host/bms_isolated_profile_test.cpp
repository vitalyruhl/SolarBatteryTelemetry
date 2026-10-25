// [MOCKED!] Radio/platform shell only; the actual Readout must deny all test controls.
#include <BmsReadout.h>
#include <cassert>
#include <cstdio>
int main() {
    hostNow = 100;
    ConfigManagerClass manager;
    cm::BluetoothManager ble;
    bms::Readout readout{"aa:bb:cc:dd:ee:ff"};
    assert(readout.attach(manager, ble));
    Config<bool>* gate = nullptr;
    for (auto* setting : manager.settings)
        if (String(setting->getKey()) == hashStringForStorage(String("BmsControl")))
            gate = dynamic_cast<Config<bool>*>(setting);
    assert(gate);
    gate->set(true);
    readout.afterLoad();
    assert(!readout.control().enabled);
    bms::MosRequest request;
    request.target = bms::MosTarget::Gate;
    request.state = true;
    request.deadline = 30100;
    assert(!readout.queueGate(request));
    request.target = bms::MosTarget::Charge;
    assert(!readout.queueRequest(request));
    readout.cancelCommands();
    readout.loop(200);
    assert(!readout.control().enabled);
    assert(ble.lastWrite.empty());
    std::puts("PASS: isolated profile denies persisted/UI/MQTT gate enable and MOS requests");
}
