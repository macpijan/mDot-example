#include "dot_util.h"
#include "RadioEvent.h"
#include "Lora.h"

/////////////////////////////////////////////////////////////
// * these options must match the settings on your gateway //
// * edit their values to match your configuration         //
// * frequency sub band is only relevant for the 915 bands //
/////////////////////////////////////////////////////////////
static uint8_t network_address[] = { 0x26, 0x01, 0x16, 0xE5 };
static uint8_t network_session_key[] = { 0x60, 0xB0, 0xD0, 0x88, 0x7F, 0xB7, 0xB8, 0x33, 0xD1, 0xE9, 0x64, 0x2C, 0xF2, 0xEE, 0xEE, 0x63 };
static uint8_t data_session_key[] = { 0x68, 0x08, 0x51, 0x18, 0xB1, 0x64, 0x17, 0xC2, 0x5B, 0x9C, 0xDA, 0x2F, 0xF6, 0xC5, 0xFC, 0x18 };
static uint8_t frequency_sub_band = 6;
static bool public_network = true;
static uint8_t ack = 0;
static bool adr = false;

mDot* dot = NULL;
lora::ChannelPlan* plan = NULL;

Serial pc(USBTX, USBRX);

AnalogIn batteryVoltage(PB_1);
AnalogIn panelVoltage(PB_0);

int main() {
    // Custom event handler for automatically displaying RX data
    RadioEvent events;

    pc.baud(115200);

    mts::MTSLog::setLogLevel(mts::MTSLog::TRACE_LEVEL);

    plan = new lora::ChannelPlan_EU868();

    assert(plan);

    dot = mDot::getInstance(plan);
    assert(dot);

    // attach the custom events handler
    dot->setEvents(&events);

    if (!dot->getStandbyFlag()) {
        logInfo("mbed-os library version: %d", MBED_LIBRARY_VERSION);

        // start from a well-known state
        logInfo("defaulting Dot configuration");
        dot->resetConfig();
        dot->resetNetworkSession();

        // make sure library logging is turned on
        dot->setLogLevel(mts::MTSLog::INFO_LEVEL);

        // update configuration if necessary
        if (dot->getJoinMode() != mDot::MANUAL) {
            logInfo("changing network join mode to MANUAL");
            if (dot->setJoinMode(mDot::MANUAL) != mDot::MDOT_OK) {
                logError("failed to set network join mode to MANUAL");
            }
        }

        update_manual_config(network_address, network_session_key, data_session_key, frequency_sub_band, public_network, ack);

        // enable or disable Adaptive Data Rate
        dot->setAdr(adr);

        // set data rate - spreading factor 7
        if (dot->getTxDataRate() != mDot::DR5) {
            logInfo("Setting TX data rate...");
        if (dot->setTxDataRate(mDot::DR5) != mDot::MDOT_OK)
            logInfo("Failed to change data rate.");
        }

        // save changes to configuration
        logInfo("saving configuration");
        if (!dot->saveConfig()) {
            logError("failed to save configuration");
        }

        // display configuration
        display_config();
    }

    const int analogChannelsUsed = 2;

    while (true) {
        uint16_t analogInputRaw[analogChannelsUsed];
        enum class AnalogVoltage { battery = 0, panel };
        std::vector<uint8_t> tx_data;

        // get some dummy data and send it to the gateway
        analogInputRaw[static_cast<int>(AnalogVoltage::battery)] = batteryVoltage.read_u16();
        analogInputRaw[static_cast<int>(AnalogVoltage::panel)] = panelVoltage.read_u16();

        for (int i = 0; i <= analogChannelsUsed - 1; i++) {
            tx_data.push_back((analogInputRaw[i] >> 8) & 0xFF);
            tx_data.push_back(analogInputRaw[i] & 0xFF);
        }

        logInfo("battery voltage: %lu [0x%04X]",
                analogInputRaw[static_cast<int>(AnalogVoltage::battery)],
                analogInputRaw[static_cast<int>(AnalogVoltage::battery)]);
        logInfo("panel voltage: %lu [0x%04X]",
                analogInputRaw[static_cast<int>(AnalogVoltage::panel)],
                analogInputRaw[static_cast<int>(AnalogVoltage::panel)]);
        send_data(tx_data);

        sleep_wake_rtc_only(false);
    }

    return 0;
}
