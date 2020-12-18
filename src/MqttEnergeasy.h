#ifndef MQTTENERGEASY_H
#define MQTTENERGEASY_H

#include <mutex>
#include <queue>
#include <string>
#include "MqttDaemon.h"
#include "Energeasy.h"

class MqttEnergeasy : public MqttDaemon
{
    public:
        MqttEnergeasy();
        ~MqttEnergeasy();

		int DaemonLoop(int argc, char* argv[]);
        void IncomingMessage(const std::string& topic, const std::string& message);

    private:
        void MessageForService(const std::string& msg);
        void MessageForDevice(const std::string& device, const std::string& msg);
        void SendMqttMessages();
        void PollEvents();
        void PollEventsThread();
        void GetStates();
        void GetStatesThread();
        bool IsEndEvent(const Json::Value& root, const std::string& execId);
        void SendEventsStates(const Json::Value& root);
        void SendStates(const std::string& deviceLabel, const Json::Value& root);

		void DaemonConfigure(SimpleIni& iniFile);
		void ConfigureFormat(SimpleIni& iniFile);

        Energeasy m_Energeasy;
        int m_StatesInterval;
		int m_PollInterval;
		int m_PollLoopCount;
		std::string m_LastExecId;
};
#endif // MQTTENERGEASY_H
