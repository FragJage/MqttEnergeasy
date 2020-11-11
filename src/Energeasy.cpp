#include "Energeasy.h"

using namespace std;

Energeasy::Energeasy(SimpleLog* log) : m_Connected(false), m_Url("https://api.energeasyconnect.com"), m_Log(log)
{
}

Energeasy::~Energeasy()
{
    Disconnect();
}

void Energeasy::SetUrl(const string& url)
{
    Disconnect();
    m_Url = url;
    if(m_Url.back()=='/') m_Url.pop_back();
}

void Energeasy::SetAuthentication(const string& user, const string& password)
{
    Disconnect();
    m_User = user;
    m_Password = password;
}

void Energeasy::SetProxy(const string& proxy)
{
    m_Proxies = cpr::Proxies{{"https", proxy}};
}

void Energeasy::SetVerifSsl(bool active)
{
    m_VerifySsl = cpr::VerifySsl{active};
}

cpr::Response Energeasy::CprGet(const string& route, bool retry)
{
    LOG_VERBOSE(m_Log) << "GET " << route;
    cpr::Response rp = cpr::Get(
                            cpr::Url{BuildUrl(route)},
                            m_Cookies,
                            m_VerifySsl,
                            m_Proxies);

    if((retry)&&(rp.status_code>=400)&&(rp.status_code<=403))
    {
        if(Connect()) CprGet(route, false);
    }

    return rp;
}

cpr::Response Energeasy::CprPost(const string& route, const string& body, bool retry)
{
    LOG_VERBOSE(m_Log) << "POST " << route;
    cpr::Response rp = cpr::Post(
                            cpr::Url{BuildUrl(route)},
                            cpr::Body{body},
                            cpr::Header{{"Content-Type", "application/json"}},
                            m_Cookies,
                            m_VerifySsl,
                            m_Proxies);

    if((retry)&&(rp.status_code>=400)&&(rp.status_code<=403))
    {
        if(Connect()) CprPost(route, body, false);
    }

    return rp;
}

bool Energeasy::CheckResponse(const cpr::Response& rp)
{
    if((rp.status_code < 200)||(rp.status_code > 299))
    {
        LOG_ERROR(m_Log) << "Call energeasy failed : " << rp.status_code << ", message " << rp.text;
        return false;
    }
    if(rp.text.empty())
    {
        LOG_ERROR(m_Log) << "Energeasy send empty response";
        return false;
    }
    return true;
}

bool Energeasy::ParseResponse(const cpr::Response& rp, Json::Value* root)
{
    string errorMsg;
    Json::CharReaderBuilder jsonReaderBuilder;
    unique_ptr<Json::CharReader> const reader(jsonReaderBuilder.newCharReader());
    if (!reader->parse(rp.text.c_str(), rp.text.c_str() + rp.text.size(), root, &errorMsg))
    {
        LOG_ERROR(m_Log) << "Failed to parse " << rp.text;
        LOG_ERROR(m_Log) << "Parse error : " << errorMsg;
        return false;
    }
    return true;
}

bool Energeasy::Connect()
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(m_Connected) Disconnect();
    Json::Value body;
    Json::StreamWriterBuilder builder;
    body["userId"] = m_User;
    body["userPassword"] = m_Password;

    cpr::Response rp = CprPost("/user/login", Json::writeString(builder, body), false);
    if(!CheckResponse(rp))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return false;
    }

    Json::Value root;
    if(!ParseResponse(rp, &root))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return false;
    }

    if(root["success"].asBool() == false)
    {
        LOG_ERROR(m_Log) << "Login on energeasy return success false " << rp.text;
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return false;
    }

    m_Cookies = cpr::Cookies({{"sessions", rp.cookies["sessions"]}}, false);
    m_Connected = true;
    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return true;
}

void Energeasy::Disconnect()
{
    if(!m_Connected) return;
    m_Connected = false;
}

bool Energeasy::RefreshSetup(bool forceRead)
{
    if((!m_Setup.empty())&&(!forceRead)) return true;
    if(!m_Connected) Connect();

    cpr::Response rp = CprGet("/api/enduser-mobile-web/enduserAPI/setup");
    if(!CheckResponse(rp))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return false;
    }

    if(!ParseResponse(rp, &m_Setup))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return false;
    }

    return true;
}

const Json::Value&  Energeasy::FindDevice(const string& deviceLabel)
{
    for(const Json::Value& deviceSetup : m_Setup["devices"])
    {
        if(deviceSetup["label"].asString() == deviceLabel) return deviceSetup;
    }

    return m_StaticEmptyValue;
}

string Energeasy::GetDevices()
{
    Json::StreamWriterBuilder wbuilder;

    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup(true))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return Json::writeString(wbuilder, m_Setup["devices"]);
}

/*
string Energeasy::GetCommands(const string& deviceLabel)
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup())
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    Json::Value device = FindDevice(deviceLabel);
    if(device.empty())
    {
        LOG_INFO(m_Log) << "Device '" << deviceLabel << "' not found.";
        LOG_DEBUG(m_Log) << "*** Exit OK ***";
        return "";
    }

    Json::StreamWriterBuilder wbuilder;
    Json::Value root;
    root["commands"] = device["definition"]["commands"];
    LOG_DEBUG(m_Log) << "*** Exit OK ***";

    return Json::writeString(wbuilder, root);
}
*/

/*
string Energeasy::GetStates(const string& deviceLabel)
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup())
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    Json::Value device = FindDevice(deviceLabel);
    if(device.empty())
    {
        LOG_INFO(m_Log) << "Device '" << deviceLabel << "' not found.";
        LOG_DEBUG(m_Log) << "*** Exit OK ***";
        return "";
    }

    Json::StreamWriterBuilder wbuilder;
    Json::Value root;
    root["states"] = device["states"];
    LOG_DEBUG(m_Log) << "*** Exit OK ***";

    return Json::writeString(wbuilder, root);
}
*/

void Energeasy::SendCommand(const string& deviceLabel, const string& jsonCommand)
{
    LOG_DEBUG(m_Log) << "*** Enter ***";

    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return;
    }

    Json::Value device = FindDevice(deviceLabel);
    if(device.empty())
    {
        LOG_INFO(m_Log) << "Device '" << deviceLabel << "' not found.";
        LOG_DEBUG(m_Log) << "*** Exit OK ***";
        return;
    }

    Json::Value body, actions, action, commands, command;
    Json::StreamWriterBuilder builder;
    Json::CharReaderBuilder jsonReaderBuilder;
    unique_ptr<Json::CharReader> const reader(jsonReaderBuilder.newCharReader());
    string errorMsg;

    if (!reader->parse(jsonCommand.c_str(), jsonCommand.c_str() + jsonCommand.size(), &command, &errorMsg))
    {
        LOG_ERROR(m_Log) << "Failed to parse " << jsonCommand;
        LOG_ERROR(m_Log) << "Parse error : " << errorMsg;
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return;
    }
    commands.append(command);

    action["commands"] = commands;
    action["deviceURL"] = device["deviceURL"];
    actions.append(action);

    body["label"] = "identification";
    body["actions"] = actions;
    body["internal"] = false;

    cpr::Response rp = CprPost("/api/enduser-mobile-web/enduserAPI/exec/apply", Json::writeString(builder, body));

    if(!CheckResponse(rp))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return;
    }

    Json::Value root;
    if(!ParseResponse(rp, &root))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return;
    }
    LOG_VERBOSE(m_Log) << root;
    LOG_DEBUG(m_Log) << "*** Exit OK ***";
}

string Energeasy::BuildUrl(const string& route)
{
    return m_Url+route;
}
