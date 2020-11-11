#include <iostream>
#include <exception>
#include "MqttEnergeasy.h"

using namespace std;

int main(int argc, char* argv[])
{
    int res = 0;

    try
    {
        MqttEnergeasy mqttEnergeasy;

        Service* pService = Service::Create("MqttEnergeasy", "Bridge between mqtt and Energeasy Box", &mqttEnergeasy);
        res = pService->Start(argc, argv);
        Service::Destroy();
    }
    catch(const exception &e)
    {
        std::cout << e.what();
    }

    return res;
}
