#include <iostream>
#include <algorithm>
#include "StringTools.h"
#include "MqttEnergeasy.h"
#include "json/json.h"

using namespace std;

MqttEnergeasy::MqttEnergeasy() : MqttDaemon("energeasy", "MqttEnergeasy"), m_Energeasy(m_Log), m_DefaultPollInterval(600), m_PollInterval(m_DefaultPollInterval)
{
}

MqttEnergeasy::~MqttEnergeasy()
{
}

void MqttEnergeasy::DaemonConfigure(SimpleIni& iniFile)
{
    LOG_ENTER;

	for (SimpleIni::SectionIterator itSection = iniFile.beginSection(); itSection != iniFile.endSection(); ++itSection)
	{
		if ((*itSection) == "mqtt") continue;
		if ((*itSection) == "log") continue;
		if ((*itSection) == "energeasy")
        {
            string value;
            value = iniFile.GetValue("energeasy", "url", "https://api.energeasyconnect.com");
            m_Energeasy.SetUrl(value);
            LOG_VERBOSE(m_Log) << "Energeasy url set to " << value;

            value = iniFile.GetValue("energeasy", "verifySsl", "");
            if(StringTools::IsEqualCaseInsensitive(value, "TRUE")||(value=="1"))
            {
                m_Energeasy.SetVerifSsl(true);
                LOG_VERBOSE(m_Log) << "Energeasy verify ssl set to true";
            }
            if(StringTools::IsEqualCaseInsensitive(value, "FALSE")||(value=="0"))
            {
                m_Energeasy.SetVerifSsl(false);
                LOG_VERBOSE(m_Log) << "Energeasy verify ssl set to false";
            }

            value = iniFile.GetValue("energeasy", "proxy", "");
            if(value!="")
            {
                m_Energeasy.SetProxy(value);
                LOG_VERBOSE(m_Log) << "Energeasy set proxy to " << value;
            }

            string user = iniFile.GetValue("energeasy", "user", "");
            string pass = iniFile.GetValue("energeasy", "password", "");
            m_Energeasy.SetAuthentication(user, pass);
            continue;
        }
    }

	LOG_EXIT_OK;
}

void MqttEnergeasy::MessageForService(const string& msg)
{
	if (msg == "GETDEVICES")
	{
        string value = m_Energeasy.GetDevices();
        if(value != "")
        {
            lock_guard<mutex> lock(m_MqttQueueAccess);
            m_MqttQueue.emplace("DEVICES", value);
        }
	}
	else
	{
		LOG_WARNING(m_Log) << "Unknown command for service " << msg;
	}
}

void MqttEnergeasy::on_message(const string& topic, const string& message)
{
	LOG_VERBOSE(m_Log) << "Mqtt receive " << topic << " : " << message;

	string mainTopic = GetMainTopic();
	if (topic.substr(0, mainTopic.length()) != mainTopic)
	{
		LOG_WARNING(m_Log) << "Receive topic not for me (" << mainTopic << ")";
		return;
	}

	if (topic.substr(mainTopic.length(), 7) != "command")
	{
		LOG_WARNING(m_Log) << "Receive topic but not a command (waiting " << mainTopic + "command" << ")";
		return;
	}

	if (topic.length() == mainTopic.length() + 7)
    {
        MessageForService(message);
        return;
    }

	string device = topic.substr(mainTopic.length() + 8);
	string execId = m_Energeasy.SendCommand(device, message);
	if(execId!="")
    {
        m_LastExecId = execId;
        m_PollInterval = 1;
        m_PollLoopCount = 0;
    }
	return;
}

void MqttEnergeasy::SendStates(const std::string& deviceLabel, const Json::Value& states)
{
    string name;
    size_t pos;

    lock_guard<mutex> lock(m_MqttQueueAccess);

    for(const Json::Value& state : states)
    {
        if(!state.isMember("name")) continue;
        if(!state.isMember("value")) continue;

        name = state["name"].asString();
        pos = name.find(":");
        if(pos != string::npos) name = name.substr(pos+1);
        m_MqttQueue.emplace(deviceLabel+"/"+name, state["value"].asString());
    }
}

void MqttEnergeasy::SendEventsStates(const Json::Value& events)
{
    string label;

    for(const Json::Value& event : events)
    {
        if(!event.isMember("deviceURL")) continue;
        if(!event.isMember("deviceStates")) continue;
        label = m_Energeasy.GetDeviceLabel(event["deviceURL"].asString());
        LOG_VERBOSE(m_Log) << "Found event states for " << label;
        if(label!="") SendStates(label, event["deviceStates"]);
    }
}

bool MqttEnergeasy::IsEndEvent(const Json::Value& events, const string& execId)
{
    string state;

    for(const Json::Value& event : events)
    {
        if(!event.isMember("execId")) continue;
        if(event["execId"].asString() != execId) continue;
        if(!event.isMember("newState")) continue;
        state = event["newState"].asString();
        if((state=="FAILED")||(state=="COMPLETED")) return true;
    }
    return false;
}

void MqttEnergeasy::PollEvents()
{
    time_t timeNow = time((time_t*)0);
    static time_t lastPoll = timeNow;

	if(timeNow-lastPoll<m_PollInterval) return;
    lastPoll = timeNow;

	LOG_ENTER;
    Json::Value root = m_Energeasy.GetEvents();
   	LOG_TRACE(m_Log) << "Received events " << root;
    SendEventsStates(root);
    if(IsEndEvent(root, m_LastExecId))
    {
        LOG_VERBOSE(m_Log) << "Found end execution of " << m_LastExecId;
        m_LastExecId = "";
        m_PollInterval = m_DefaultPollInterval;
    }

    if(m_LastExecId != "")
    {
        m_PollLoopCount++;
        if(m_PollLoopCount>29) m_PollInterval = 2;
        if(m_PollLoopCount>60)
        {
            m_LastExecId = "";
            m_PollInterval = m_DefaultPollInterval;
        }
    }
	LOG_VERBOSE(m_Log) << "Pool interval " << m_PollInterval << "s";
  	LOG_EXIT_OK;
}

int MqttEnergeasy::DaemonLoop(int argc, char* argv[])
{

	LOG_ENTER;

	Subscribe(GetMainTopic() + "command/#");
	LOG_VERBOSE(m_Log) << "Subscript to : " << GetMainTopic() + "command/#";

	bool bStop = false;
	m_bPause = false;
	while(!bStop)
    {
		int cond = Service::Get()->WaitFor({ m_MqttQueueCond }, 250);
		if (cond == Service::STATUS_CHANGED)
		{
			switch (Service::Get()->GetStatus())
			{
                case Service::StatusKind::PAUSE:
                    m_bPause = true;
                    break;
                case Service::StatusKind::START:
                    m_bPause = false;
                    break;
                case Service::StatusKind::STOP:
                    bStop = true;
                    break;
			}
		}
		if (!m_bPause)
		{
		    PollEvents();
			SendMqttMessages();
		}
    }

	LOG_EXIT_OK;
    return 0;
}

void MqttEnergeasy::SendMqttMessages()
{
	lock_guard<mutex> lock(m_MqttQueueAccess);
	while (!m_MqttQueue.empty())
	{
		MqttQueue& mqttQueue = m_MqttQueue.front();
		LOG_VERBOSE(m_Log) << "Send " << mqttQueue.Topic << " : " << mqttQueue.Message;
		Publish(mqttQueue.Topic, mqttQueue.Message);
		m_MqttQueue.pop();
	}
}
