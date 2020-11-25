#ifndef ENERGEASY_H
#define ENERGEASY_H

#include <string>
#include <set>
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
        Json::Value GetDevices();
        std::set<std::string> GetDevicesUrl();
        Json::Value GetStates(const std::string& deviceUrl);
        std::string SendCommand(const std::string& deviceUrl, const std::string& jsonCommand);
        std::string GetDeviceUrlFromLabel(const std::string& deviceLabel);
        std::string GetDeviceLabelFromUrl(const std::string& deviceUrl);
        bool PollStart();
        void PollStop();
        Json::Value PollEvents();

    protected:

    private:
        std::string BuildUrl(const std::string& route);
        cpr::Response CprGet(const std::string& route, bool retry=true);
        cpr::Response CprPost(const std::string& route, const std::string& body, bool retry=true);
        const Json::Value& FindCachedDevice(const std::string& deviceUrl);
        bool CheckResponse(const cpr::Response& rp);
        bool ParseResponse(const cpr::Response& rp, Json::Value* root);
        bool RefreshSetup(bool forceRead);
        bool RegisterEvents();
        void UnregisterEvents();

        bool m_Connected;
        std::string m_Url;
        std::string m_User;
        std::string m_Password;
        std::string m_RegisterEventId;
        cpr::Cookies m_Cookies;
        cpr::VerifySsl m_VerifySsl;
        cpr::Proxies m_Proxies;
        time_t m_SetupRead;
        Json::Value m_Setup;
        SimpleLog* m_Log;
        Json::Value m_StaticEmptyValue;
};

#endif // ENERGEASY_H
