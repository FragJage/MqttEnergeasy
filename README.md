# MqttEnergeasy
Under construction, but you can already :

- Get all devices from energeasy box with topic `energeasy/command` message `GETDEVICES`
- Send command to device with topic `energeasy/command/>DeviceLabel<` message `{"name":">CommandName<", parameters:[]}`
- Set your authentication in config file and copy on /etc.
