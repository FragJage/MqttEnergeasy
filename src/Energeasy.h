#ifndef ENERGEASY_H
#define ENERGEASY_H

#include <string>
#include <list>
#include "SimpleLog.h"
#include "cpr/cpr.h"
#include "json/json.h"

class Energeasy
{
    public:
        Energeasy(SimpleLog* log);
        ~Energeasy();

        void SetUrl(const std::string& url);
        void SetAuthentication(const std::string& user, const std::string& password);
        void SetProxy(const std::string& proxy);
        void SetVerifSsl(bool active);
        bool Connect();
        void Disconnect();
        std::string GetDevices();
        //std::string GetCommands(const std::string& deviceLabel);
        //std::string GetStates(const std::string& deviceLabel);
        void SendCommand(const std::string& deviceLabel, const std::string& jsonCommand);

    protected:

    private:
        std::string BuildUrl(const std::string& route);
        cpr::Response CprGet(const std::string& route, bool retry=true);
        cpr::Response CprPost(const std::string& route, const std::string& body, bool retry=true);
        bool CheckResponse(const cpr::Response& rp);
        bool ParseResponse(const cpr::Response& rp, Json::Value* root);
        bool RefreshSetup(bool forceRead);
        const Json::Value&  FindDevice(const std::string& deviceLabel);
        bool m_Connected;
        std::string m_Url;
        std::string m_User;
        std::string m_Password;
        cpr::Cookies m_Cookies;
        cpr::VerifySsl m_VerifySsl;
        cpr::Proxies m_Proxies;
        Json::Value m_Setup;
        SimpleLog* m_Log;
        Json::Value m_StaticEmptyValue;
};

#endif // ENERGEASY_H