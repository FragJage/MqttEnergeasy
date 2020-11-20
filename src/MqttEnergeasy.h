#ifndef MQTTENERGEASY_H
#define MQTTENERGEASY_H

#include <mutex>
#include <queue>
#include <string>
#include "MqttDaemon.h"
#include "Energeasy.h"

struct MqttQueue
{
	MqttQueue(std::string topic, std::string msg) : Topic(topic), Message(msg) {};
	std::string Topic;
	std::string Message;
};

class MqttEnergeasy : public MqttDaemon
{
    public:
        MqttEnergeasy();
        ~MqttEnergeasy();

		int DaemonLoop(int argc, char* argv[]);
        void on_message(const std::string& topic, const std::string& message);

    private:
        void MessageForService(const std::string& msg);
        void MessageForDevice(const std::string& device, const std::string& msg);
        void SendMqttMessages();
        void PollEvents();
        bool IsEndEvent(const Json::Value& root, const std::string& execId);
        void SendEventsStates(const Json::Value& root);
        void SendStates(const std::string& deviceLabel, const Json::Value& root);

		void DaemonConfigure(SimpleIni& iniFile);
		void ConfigureFormat(SimpleIni& iniFile);

        bool m_bPause;
        Energeasy m_Energeasy;
		int m_DefaultPollInterval;
		int m_PollInterval;
		int m_PollLoopCount;
		std::string m_LastExecId;
		std::mutex m_MqttQueueAccess;
		ServiceConditionVariable m_MqttQueueCond;
		std::queue<MqttQueue> m_MqttQueue;
};
#endif // MQTTENERGEASY_H
