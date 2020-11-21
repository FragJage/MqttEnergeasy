# MqttEnergeasy
Under construction, but you can already :

- Get all devices from energeasy box with topic `energeasy/command` message `GETDEVICES`
- Send command to device with topic `energeasy/command/>DeviceLabel<` message `{"name":">CommandName<", parameters:[]}`
- Refresh states of a device with topic `energeasy/command/>DeviceLabel<` message `REFRESH`
- Set your authentication in config file (MqttEnergeasy.conf) and copy to /etc.

All devices states are refeshed every 600 secondes.

When a command is sended to a device, there is a polling to get events until the command is complete. The pooling repeats 30 times at one second interval then 30 times at two second interval, so the command must be complete in 90 seconds.
