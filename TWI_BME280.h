/*
    yet another i2c bus library for bme280 trying to be smaller (but isn't, thanks cpp classes).
    focus on imperial units but there's some metric things too. Using burst address
    reads since the datasheet stresses it being better, even if you only want 2 things.
    return values are preformatted floats

    Inspired by TinyBME280 and CORE_BME280_I2C

    Copyright (C) lemonforest 6/24/2019 @ 21:03 CDT -- MIT License
 */

#ifndef __TWI_BME280_H__
#define __TWI_BME280_H__

#include "Arduino.h"

#define SEALEVELPRESSURE 101325.0F

#define BME280_I2C_ADDR 0x76        // set a default address
#define BME280_CHIPID 0x60

/*
    bme280 compensation registers
 */

#define BME280_DIG_T1_REG 0x88
#define BME280_DIG_T2_REG 0x8A
#define BME280_DIG_T3_REG 0x8C

#define BME280_DIG_P1_REG 0x8E
#define BME280_DIG_P2_REG 0x90
#define BME280_DIG_P3_REG 0x92
#define BME280_DIG_P4_REG 0x94
#define BME280_DIG_P5_REG 0x96
#define BME280_DIG_P6_REG 0x98
#define BME280_DIG_P7_REG 0x9A
#define BME280_DIG_P8_REG 0x9C
#define BME280_DIG_P9_REG 0x9E

#define BME280_DIG_H1_REG 0xA1
#define BME280_DIG_H2_REG 0xE1
#define BME280_DIG_H3_REG 0xE3
#define BME280_DIG_H4_REG 0xE4
#define BME280_DIG_H5_REG 0xE5
#define BME280_DIG_H6_REG 0xE7

/*
    bme280 data registers
 */

#define BME280_REGISTER_CHIPID          0xD0
#define BME280_REGISTER_VERSION         0xD1
#define BME280_REGISTER_SOFTRESET       0xE0
#define BME280_REGISTER_CAL26           0xE1
#define BME280_REGISTER_CONTROLHUMID    0xF2
#define BME280_REGISTER_STATUS          0xF3
#define BME280_REGISTER_CONTROL         0xF4
#define BME280_REGISTER_CONFIG          0xF5
#define BME280_REGISTER_DATA_PRESSURE   0xF7
#define BME280_REGISTER_DATA_TEMP       0xFA
#define BME280_REGISTER_DATA_HUMID      0xFD

/*
    bme280 control registers.
    write humidity related values to 0xF2 or BME280_REGISTER_CONTROLHUMID
    0xF2 [2:0]oversample humidity

    read only BME280_REGISTER_STATUS
    0xF3 [3]measuring=1 if conversion is running
         [0]im_update=1 if NVM data is being copied to registers

    write temp, pressure, and mode related values to 0xF4 or BME280_REGISTER_CONTROL after BME280_REGISTER_CONTROLHUMID
    0xF4 [7:5]oversample temperature
         [4:2]oversample pressure
         [1:0]sensor mode
 */

#define BME280_COMMAND_RESET                0xB6

#define BME280_COMMAND_OVERSAMPLING_H_NO    0x00
#define BME280_COMMAND_OVERSAMPLING_H_X1    0x01
#define BME280_COMMAND_OVERSAMPLING_H_X2    0x02
#define BME280_COMMAND_OVERSAMPLING_H_X4    0x03
#define BME280_COMMAND_OVERSAMPLING_H_X8    0x04
#define BME280_COMMAND_OVERSAMPLING_H_X16   0x05

#define BME280_COMMAND_STATUS_MEASURING     0x08
#define BME280_COMMAND_STATUS_UPDATING      0X01

#define BME280_COMMAND_OVERSAMPLING_P_NO    0x00
#define BME280_COMMAND_OVERSAMPLING_P_X1    0x04
#define BME280_COMMAND_OVERSAMPLING_P_X2    0x08
#define BME280_COMMAND_OVERSAMPLING_P_X4    0x0C
#define BME280_COMMAND_OVERSAMPLING_P_X8    0x10
#define BME280_COMMAND_OVERSAMPLING_P_X16   0x14

#define BME280_COMMAND_OVERSAMPLING_T_NO    0x00
#define BME280_COMMAND_OVERSAMPLING_T_X1    0x20
#define BME280_COMMAND_OVERSAMPLING_T_X2    0x40
#define BME280_COMMAND_OVERSAMPLING_T_X4    0x60
#define BME280_COMMAND_OVERSAMPLING_T_X8    0x80
#define BME280_COMMAND_OVERSAMPLING_T_X16   0xA0

#define BME280_COMMAND_MODE_SLEEP           0x00
#define BME280_COMMAND_MODE_FORCED          0x01
#define BME280_COMMAND_MODE_NORMAL          0x03

/*
    interface configuration commands written to BME280_REGISTER_CONFIG
    0xF5 [7:5]inactive duration in normal mode [0.5, 62.5, 125, 250, 500, 1000, 10, 20]ms per 5.4.6
         [4:2]IIR filter time constant
         [0]3-wire SPI if = 1
 */

#define BME280_MODE_NORMAL_INACTIVE_L1      0x00    //0.5ms
#define BME280_MODE_NORMAL_INACTIVE_L2      0x20    //62.5ms
#define BME280_MODE_NORMAL_INACTIVE_L3      0x40    //125ms
#define BME280_MODE_NORMAL_INACTIVE_L4      0x60    //250ms
#define BME280_MODE_NORMAL_INACTIVE_L5      0x80    //500ms
#define BME280_MODE_NORMAL_INACTIVE_L6      0xA0    //1000ms
#define BME280_MODE_NORMAL_INACTIVE_L7      0xC0    //10ms
#define BME280_MODE_NORMAL_INACTIVE_L8      0xE0    //20ms

#define BME280_MODE_IIR_COEFFICIENT_NONE    0x00
#define BME280_MODE_IIR_COEFFICIENT_2       0x04
#define BME280_MODE_IIR_COEFFICIENT_4       0x08
#define BME280_MODE_IIR_COEFFICIENT_8       0x0C
#define BME280_MODE_IIR_COEFFICIENT_16      0x10

/*
    temperature/pressure/humidity memory map

    0xF7 [7:0]=pressure [19:12]
    0xF8 [7:0]=pressure [11:4]
    0xF9 [7:4]=pressure [3:0]

    0xFA [7:0]=temperature [19:12]
    0xFB [7:0]=temperature [11:4]
    0xFC [7:4]=temperature [3:0]

    0xFD [7:0]=humidity [15:8]
    0xFE [7:0]=humidity [7:0]

 */

/*
    factory calibration data vars
 */

struct BME280_Factory_Calibration {
    public:
        
        uint16_t dig_T1;
        int16_t  dig_T2;
        int16_t  dig_T3;
        
        uint16_t dig_P1;
        int16_t  dig_P2;
        int16_t  dig_P3;
        int16_t  dig_P4;
        int16_t  dig_P5;
        int16_t  dig_P6;
        int16_t  dig_P7;
        int16_t  dig_P8;
        int16_t  dig_P9;
        
        uint8_t  dig_H1;
        int16_t  dig_H2;
        uint8_t  dig_H3;
        int16_t  dig_H4;
        int16_t  dig_H5;
        int8_t   dig_H6;
};

class TWI_BME280 {

public:
    TWI_BME280(void);           // use default address
    TWI_BME280(uint8_t);        // use user specified address

    bool begin(void);

    void setTempOffset(uint8_t);    // set temp offset in celsius
    
    void readSensor(void);          // maybe make it share function names with a more common library

    float getTemperature_C(void);  // return temperature in celsius
    float getTemperature_F(void);  // return temperature in fahrenheit
    float getPressure_HP(void);     // return pressure in hPa
    float getPressure_Pa(void);
    float getPressure_inHg(void);   // return pressure in inHg
    float getHumidity(void);     // return humidity
    float getAltitude_m(void);  // altitude in meters
    float getAltitude_ft(void); // altitude in feet

private:
    BME280_Factory_Calibration calibrationData;     // calibration/compensation data

    uint8_t _i2caddr;
    float tempOffset;             // stores user defined temp offset
    
    float temperature,pressure, humidity;
    int32_t t_fine;

    void compensateTemperature(void);   //compensate temperature
    void compensatePressure(void);  //compensate pressure
    void compensateHumidity(void);  //compensate humidity

    void write8(byte reg, byte value);
    uint8_t read8(byte reg);
    uint16_t read16(void);
    uint16_t read16_LE(void);
    inline uint16_t read12(void);
    inline uint16_t read12_LE(void);
    uint32_t read20(void);
    bool beginWireRead(uint8_t, uint8_t);

};

#endif