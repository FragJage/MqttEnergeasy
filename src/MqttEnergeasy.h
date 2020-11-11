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
        void SendMqttMessages();
        bool m_bPause;
        Energeasy m_Energeasy;

		void DaemonConfigure(SimpleIni& iniFile);
		void ConfigureFormat(SimpleIni& iniFile);

		std::mutex m_MqttQueueAccess;
		ServiceConditionVariable m_MqttQueueCond;
		std::queue<MqttQueue> m_MqttQueue;
};
#endif // MQTTENERGEASY_H
