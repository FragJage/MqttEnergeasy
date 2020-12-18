#include <iostream>
#include <algorithm>
#include <thread>
#include "StringTools.h"
#include "MqttEnergeasy.h"
#include "json/json.h"

using namespace std;

MqttEnergeasy::MqttEnergeasy() : MqttDaemon("energeasy", "MqttEnergeasy"), m_Energeasy(m_Log), m_StatesInterval(600)
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

void MqttEnergeasy::MessageForDevice(const std::string& deviceLabel, const std::string& msg)
{
    string deviceUrl = m_Energeasy.GetDeviceUrlFromLabel(deviceLabel);
    if(deviceUrl == "")
    {
        LOG_INFO(m_Log) << "No device url found for label " << deviceLabel;
        return;
    }

    if(msg=="REFRESH")
    {
        Json::Value states = m_Energeasy.GetStates(deviceUrl);
        SendStates(deviceLabel, states);
        return;
    }

	string execId = m_Energeasy.SendCommand(deviceUrl, msg);
	if(execId!="")
    {
        if(m_Energeasy.PollStart())
        {
            m_LastExecId = execId;
            m_PollInterval = 1;
            m_PollLoopCount = 0;
        }
    }
}

void MqttEnergeasy::MessageForService(const string& msg)
{
	if (msg == "GETDEVICES")
	{
        Json::Value value = m_Energeasy.GetDevices();
        if(!value.empty())
        {
            Json::StreamWriterBuilder wbuilder;
            PublishAsyncAdd("DEVICES", Json::writeString(wbuilder, value));
            PublishAsyncStart();
        }
	}
	else
	{
		LOG_WARNING(m_Log) << "Unknown command for service " << msg;
	}
}

void MqttEnergeasy::IncomingMessage(const string& topic, const string& message)
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
	MessageForDevice(device, message);
	return;
}

void MqttEnergeasy::SendStates(const std::string& deviceLabel, const Json::Value& states)
{
    string name;
    size_t pos;
    bool send = false;

    for(const Json::Value& state : states)
    {
        if(!state.isMember("name")) continue;
        if(!state.isMember("value")) continue;

        name = state["name"].asString();
        pos = name.find(":");
        if(pos != string::npos) name = name.substr(pos+1);
        PublishAsyncAdd(deviceLabel+"/"+name, state["value"].asString());
        send = true;
    }
    if(send) PublishAsyncStart();
}

void MqttEnergeasy::SendEventsStates(const Json::Value& events)
{
    string label;

    for(const Json::Value& event : events)
    {
        if(!event.isMember("deviceURL")) continue;
        if(!event.isMember("deviceStates")) continue;
        label = m_Energeasy.GetDeviceLabelFromUrl(event["deviceURL"].asString());
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

    thread t(&MqttEnergeasy::PollEventsThread, this);
    t.detach();
}

void MqttEnergeasy::PollEventsThread()
{
    static bool alreadyLaunch = false;   //No mutex protection, 250ms minimum between two launchs

	LOG_ENTER;
	if(alreadyLaunch)
    {
        LOG_INFO(m_Log) << "Thread already launch";
        return;
    }
    alreadyLaunch = true;

    Json::Value root = m_Energeasy.PollEvents();
   	LOG_TRACE(m_Log) << "Received events " << root;
    SendEventsStates(root);
    if(IsEndEvent(root, m_LastExecId))
    {
        LOG_VERBOSE(m_Log) << "Found end execution of " << m_LastExecId;
        m_LastExecId = "";
    }
    else
    {
        m_PollLoopCount++;
        if(m_PollLoopCount>29)
        {
            m_PollInterval = 2;
            LOG_VERBOSE(m_Log) << "Poll interval set to " << m_PollInterval << "s.";
        }
        if(m_PollLoopCount>60)
        {
            m_LastExecId = "";
            LOG_VERBOSE(m_Log) << "Forced stop polling !";
        }
    }

    alreadyLaunch = false;
  	LOG_EXIT_OK;
}

void MqttEnergeasy::GetStates()
{
    time_t timeNow = time((time_t*)0);
    static time_t lastPoll = timeNow-m_StatesInterval;

	if(timeNow-lastPoll<m_StatesInterval) return;
    lastPoll = timeNow;

    thread t(&MqttEnergeasy::GetStatesThread, this);
    t.detach();
}

void MqttEnergeasy::GetStatesThread()
{
    static bool alreadyLaunch = false;   //No mutex protection, 250ms minimum between two launchs

	LOG_ENTER;
	if(alreadyLaunch)
    {
        LOG_INFO(m_Log) << "Thread already launch";
        return;
    }
    alreadyLaunch = true;

	string label;
	set<string> urls = m_Energeasy.GetDevicesUrl();
	for(const string& deviceUrl : urls)
    {
        label = m_Energeasy.GetDeviceLabelFromUrl(deviceUrl);
        if(label.find("#") != string::npos) continue;
        if(label.find("+") != string::npos) continue;
        Json::Value states = m_Energeasy.GetStates(deviceUrl);
        SendStates(label, states);
    }

    alreadyLaunch = false;
  	LOG_EXIT_OK;
}

int MqttEnergeasy::DaemonLoop(int argc, char* argv[])
{
	LOG_ENTER;

	Subscribe(GetMainTopic() + "command/#");
	LOG_VERBOSE(m_Log) << "Subscript to : " << GetMainTopic() + "command/#";

	bool bStop = false;
	while(!bStop)
    {
        if(WaitFor(250)==Service::STATUS_CHANGED)
        {
            if(Service::Get()->GetStatus() == Service::StatusKind::STOP) bStop = true;
        }
        if(m_LastExecId!="") PollEvents();
        GetStates();
    }

	LOG_EXIT_OK;
    return 0;
}
