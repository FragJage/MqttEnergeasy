#include "Energeasy.h"

using namespace std;

Energeasy::Energeasy(SimpleLog* log) : m_Connected(false), m_Url("https://api.energeasyconnect.com"), m_Log(log)
{
}

Energeasy::~Energeasy()
{
    if(m_RegisterEventId != "") PollStop();
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
                            cpr::Header{{"Content-Type", "application/json;charset=UTF-8"}},
                            m_Cookies,
                            m_VerifySsl,
                            m_Proxies);

    if((retry)&&((rp.status_code==400)||(rp.status_code==411)))
    {
        if(Connect()) rp = CprGet(route, false);
    }

    return rp;
}

cpr::Response Energeasy::CprPost(const string& route, const string& body, bool retry)
{
    LOG_VERBOSE(m_Log) << "POST " << route;
    cpr::Response rp = cpr::Post(
                            cpr::Url{BuildUrl(route)},
                            cpr::Body{body},
                            cpr::Header{{"Content-Type", "application/json;charset=UTF-8"}},
                            m_Cookies,
                            m_VerifySsl,
                            m_Proxies);

    if((retry)&&((rp.status_code==400)||(rp.status_code==411)))
    {
        if(Connect()) rp = CprPost(route, body, false);
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
        LOG_VERBOSE(m_Log) << "Failed to parse " << rp.text;
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
    if(!m_Connected)
    {
        if(!Connect()) return false;
    }

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

    m_SetupRead = time((time_t*)0);
    return true;
}

string Energeasy::GetDeviceUrlFromLabel(const string& deviceLabel)
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    string deviceUrl = "";

    for(const Json::Value& deviceSetup : m_Setup["devices"])
    {
        if(deviceSetup["label"].asString() == deviceLabel)
        {
            deviceUrl = deviceSetup["deviceURL"].asString();
            break;
        }
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return deviceUrl;
}

string Energeasy::GetDeviceLabelFromUrl(const string& deviceUrl)
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    string deviceLabel = "";

    for(const Json::Value& deviceSetup : m_Setup["devices"])
    {
        if(deviceSetup["deviceURL"].asString() == deviceUrl)
        {
            deviceLabel = deviceSetup["label"].asString();
            break;
        }
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return deviceLabel;
}

set<string> Energeasy::GetDevicesUrl()
{
    set<string> urls;

    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return urls;
    }

    for(const Json::Value& deviceSetup : m_Setup["devices"])
    {
        urls.insert(deviceSetup["deviceURL"].asString());
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return urls;
}

Json::Value Energeasy::GetDevices()
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return m_Setup["devices"];
}

bool Energeasy::PollStart()
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    if(m_RegisterEventId != "") PollStop();

    Json::Value body;
    Json::StreamWriterBuilder builder;
    body["action"] = true;

    cpr::Response rp = CprPost("/api/enduser-mobile-web/enduserAPI/events/register", "", true);
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

    m_RegisterEventId = root["id"].asString();

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return true;
}

void Energeasy::PollStop()
{
    LOG_DEBUG(m_Log) << "*** Enter ***";

    if(m_RegisterEventId == "")
    {
        LOG_DEBUG(m_Log) << "*** Exit OK ***";
        return;
    }

    CprPost("/api/enduser-mobile-web/enduserAPI/events/"+m_RegisterEventId+"/unregister", "", false);

    m_RegisterEventId = "";
    LOG_DEBUG(m_Log) << "*** Exit OK ***";
}

Json::Value Energeasy::PollEvents()
{
    LOG_DEBUG(m_Log) << "*** Enter ***";
    Json::Value root;

    if(!m_Connected)
    {
        if(!Connect())
        {
            LOG_ERROR(m_Log) << "Unable to connect.";
            LOG_DEBUG(m_Log) << "*** Exit KO ***";
            return root;
        }
    }

    if(m_RegisterEventId == "")
    {
        if(!PollStart())
        {
            LOG_ERROR(m_Log) << "Unable to get register event id.";
            LOG_DEBUG(m_Log) << "*** Exit KO ***";
            return root;
        }
    }

    cpr::Response rp = CprPost("/api/enduser-mobile-web/enduserAPI/events/"+m_RegisterEventId+"/fetch", "", true);
    if(!CheckResponse(rp))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return root;
    }

    if(!ParseResponse(rp, &root))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return false;
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return root;
}

const Json::Value& Energeasy::FindCachedDevice(const string& deviceUrl)
{
    for(const Json::Value& deviceSetup : m_Setup["devices"])
    {
        if(deviceSetup["deviceURL"].asString() == deviceUrl) return deviceSetup;
    }

    return m_StaticEmptyValue;
}

Json::Value Energeasy::GetStates(const string& deviceUrl)
{
    Json::Value root;
    LOG_DEBUG(m_Log) << "*** Enter ***";
    LOG_VERBOSE(m_Log) << "Get states of "<< deviceUrl;

    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return root;
    }

    time_t timeNow = time((time_t*)0);
    if(timeNow-m_SetupRead<10)
    {
        Json::Value device = FindCachedDevice(deviceUrl);
        if(!device.empty())
        {
            LOG_VERBOSE(m_Log) << "Return cached states";
            LOG_DEBUG(m_Log) << "*** Exit OK ***";
            return device["states"];
        }
    }

    //Oui, oui, j'encode deux fois
    string encoded = cpr::util::urlEncode(cpr::util::urlEncode(deviceUrl));
    cpr::Response rp = CprGet("/api/enduser-mobile-web/enduserAPI/setup/devices/"+encoded+"/states");

    if(!CheckResponse(rp))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return root;
    }

    if(!ParseResponse(rp, &root))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return root;
    }

    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return root;
}

string Energeasy::SendCommand(const string& deviceUrl, const string& jsonCommand)
{
    LOG_DEBUG(m_Log) << "*** Enter ***";

    if(!RefreshSetup(false))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    Json::Value body, actions, action, commands, command;
    Json::StreamWriterBuilder builder;
    Json::CharReaderBuilder jsonReaderBuilder;
    unique_ptr<Json::CharReader> const reader(jsonReaderBuilder.newCharReader());
    string errorMsg;

    if (!reader->parse(jsonCommand.c_str(), jsonCommand.c_str() + jsonCommand.size(), &command, &errorMsg))
    {
        LOG_VERBOSE(m_Log) << "Failed to parse " << jsonCommand;
        LOG_ERROR(m_Log) << "Parse error : " << errorMsg;
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }
    commands.append(command);

    action["commands"] = commands;
    action["deviceURL"] = deviceUrl;
    actions.append(action);

    body["label"] = "identification";
    body["actions"] = actions;
    body["internal"] = false;

    cpr::Response rp = CprPost("/api/enduser-mobile-web/enduserAPI/exec/apply", Json::writeString(builder, body));

    if(!CheckResponse(rp))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }

    Json::Value root;
    if(!ParseResponse(rp, &root))
    {
        LOG_DEBUG(m_Log) << "*** Exit KO ***";
        return "";
    }
    LOG_DEBUG(m_Log) << "*** Exit OK ***";
    return root["execId"].asString();
}

string Energeasy::BuildUrl(const string& route)
{
    return m_Url+route;
}
