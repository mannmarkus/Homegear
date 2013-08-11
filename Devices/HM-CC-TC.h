#ifndef HM_CC_TC_H
#define HM_CC_TC_H

#include <thread>
#include <memory>
#include <vector>

class BidCoSQueue;
enum class BidCoSQueueType;

#include "../HomeMaticDevice.h"

class HM_CC_TC : public HomeMaticDevice
{
    public:
        HM_CC_TC();
        HM_CC_TC(std::string, int32_t);
        virtual ~HM_CC_TC();

        void setValveState(int32_t valveState);
        int32_t getNewValueState() { return _newValveState; }
        void handleCLICommand(std::string command);
        void stopThreads();

        void handlePairingRequest(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
        void handleConfigParamResponse(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
        void handleAck(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
        void handleSetPoint(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
        void handleSetValveState(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
        void handleConfigPeerAdd(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
        std::string serialize();
        void unserialize(std::string serializedObject, uint8_t dutyCycleMessageCounter, int64_t lastDutyCycleEvent);
    protected:
        virtual void setUpBidCoSMessages();
        virtual void init();
    private:
        int32_t _currentDutyCycleDeviceAddress = -1;
        int32_t _temperature = 213;
        int32_t _setPointTemperature = 42;
        int32_t _humidity = 56;
        bool _stopDutyCycleThread = false;
        std::shared_ptr<Peer> createPeer(int32_t address, int32_t firmwareVersion, HMDeviceTypes deviceType, std::string serialNumber, int32_t remoteChannel, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet = std::shared_ptr<BidCoSPacket>());
        std::thread* _dutyCycleThread = nullptr;
        int32_t _valveState = 0;
        int32_t _newValveState = 0;
        int32_t _dutyCycleCounter  = 0;
        bool _dutyCycleBroadcast = false;
        void reset();

        int32_t getNextDutyCycleDeviceAddress();
        int64_t calculateLastDutyCycleEvent();
        int32_t getAdjustmentCommand();

        void sendRequestConfig(int32_t messageCounter, int32_t controlByte, std::shared_ptr<BidCoSPacket> packet);
        void sendConfigParams(int32_t messageCounter, int32_t controlByte, std::shared_ptr<BidCoSPacket> packet);
        void sendDutyCycleBroadcast();
        void sendDutyCyclePacket(uint8_t messageCounter, int64_t sendingTime);
        void startDutyCycle(int64_t lastDutyCycleEvent);
        void dutyCycleThread(int64_t lastDutyCycleEvent);
        void setUpConfig();
};

#endif // HM_CC_TC_H
