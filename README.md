# ESPhome MTP40F Component

This ESPhome component is designed for using the Tuya CO2 sensor with ESPhome.

## Custom Components

### mtp40f

![image](https://github.com/user-attachments/assets/4f413618-9613-429e-9ac8-0ac42dc5b43b)

This sensor is used with the Tuya CO2 sensor.

Comparison with MHZ-19 

No temperature sensor, uses pressure value as a supplementary indicator

### datasheet

https://robu.in/wp-content/uploads/2024/01/DataSheet-5.pdf

### Available Features:

 - Automatic baseline calibration activation/deactivation
 - Zero baseline calibration
 - Set warming up time 

### Unavailable Features:

 - Read reference pressure value
 - enter reference pressure value

### Configurations

**General**
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/plplaaa2/esphome_mtp40f
      ref: dev
    components: [ mtp40f ]
```

Example config 

```
sensor:
  - platform: mtp40f
    id: mtp40f_component
    co2:
      name: "CO2 Level"      
      unit_of_measurement: "ppm"
    self_calibration: true
    warmup_time: 60s
    update_interval: 10s
    uart_id: uart_mhz19

switch:
  - platform: mtp40f
    name: "MTP40F Self Calibration"
    mtp40f_id: mtp40f_component

button:
  - platform: template
    name: "Mtp40f Zero Base Calibrate"
    on_press: 
      then:
        - mtp40f.calibrate_400ppm: mtp40f_component
```
