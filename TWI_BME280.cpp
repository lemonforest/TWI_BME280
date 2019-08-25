/*
    yet another i2c bus library for bme280 trying to be smaller (but isn't, thanks cpp classes).
    focus on imperial units but there's some metric things too. Using burst address
    reads since the datasheet stresses it being better, even if you only want 2 things.
    return values are preformatted floats

    Inspired by TinyBME280 and CORE_BME280_I2C

    Copyright (C) lemonforest 6/24/2019 @ 21:03 CDT -- MIT License
 */

#include "TWI_BME280.h"

#include <Wire.h>

/*
    public functions
 */

TWI_BME280::TWI_BME280(void) {

    _i2caddr = BME280_I2C_ADDR;

    tempOffset = 0.0;
    temperature = 0.0;
    pressure = 0.0;
    humidity = 0.0;
}

TWI_BME280::TWI_BME280(uint8_t addr) {

    _i2caddr = addr;

    tempOffset = 0.0;
    temperature = 0.0;
    pressure = 0.0;
    humidity = 0.0;
}

bool TWI_BME280::begin() {

    uint8_t h4, h5;

    Wire.begin();

    if(read8(BME280_REGISTER_CHIPID)!=BME280_CHIPID)
        
        return false;   //not a bme280

    // read compensation registers
    // read calib00 to calib25. first register is lsb

    beginWireRead((uint8_t)BME280_DIG_T1_REG, (uint8_t)26);

    calibrationData.dig_T1 = read16_LE();
    calibrationData.dig_T2 = (int16_t)read16_LE();
    calibrationData.dig_T3 = (int16_t)read16_LE();

    calibrationData.dig_P1 = read16_LE();
    calibrationData.dig_P2 = (int16_t)read16_LE();
    calibrationData.dig_P3 = (int16_t)read16_LE();
    calibrationData.dig_P4 = (int16_t)read16_LE();
    calibrationData.dig_P5 = (int16_t)read16_LE();
    calibrationData.dig_P6 = (int16_t)read16_LE();
    calibrationData.dig_P7 = (int16_t)read16_LE();
    calibrationData.dig_P8 = (int16_t)read16_LE();
    calibrationData.dig_P9 = (int16_t)read16_LE();

    Wire.read(); // nothing in 0xA0 but need 0xA1
    calibrationData.dig_H1 = Wire.read();
    
    // now read 0xE1 to 0xE7
    beginWireRead((uint8_t)BME280_DIG_H2_REG, (uint8_t)7);

    calibrationData.dig_H2 = (int16_t)read16_LE();
    calibrationData.dig_H3 = Wire.read();
    h4 = Wire.read(); h5 = Wire.read();   // H4 & H5 use values from the same address space
    calibrationData.dig_H4 = (int16_t)(h4<<4|(h5&0x0F));
    calibrationData.dig_H5 = (int16_t)((Wire.read())<<4|h5>>4);
    calibrationData.dig_H6 = (int8_t)Wire.read();

    return true;
}

void TWI_BME280::setTempOffset(uint8_t offset) {
    
    tempOffset = offset;
}

float TWI_BME280::getTemperature_C(void) {
    
    return temperature + (tempOffset);
}

float TWI_BME280::getTemperature_F(void) {
    
    return (temperature +(tempOffset))*9/5+32;
}

uint16_t TWI_BME280::getTemperature(void) {

    return temp;
}

float TWI_BME280::getPressure_HP(void) {
    
    return pressure/100.0;
}

float TWI_BME280::getPressure_inHg(void) {

    return pressure * 0.00029529983071445;
}

float TWI_BME280::getPressure_Pa(void) {

    return pressure;
}

uint32_t TWI_BME280::getPressure(void) {

    return press;
}

float TWI_BME280::getHumidity(void) {

    return humidity;
}

uint32_t TWI_BME280::getHumidity_int(void) {

    return hum;
}

float TWI_BME280::getAltitude_m(void) {

    return (-45846.2F)*(pow((pressure / SEALEVELPRESSURE), 0.190263F) - 1.0F);
}

// too much math for the attiny85 :(
float TWI_BME280::getAltitude_ft(void) {

    return (-45846.2F)*(pow((pressure / SEALEVELPRESSURE), 0.190263F) - 1.0F)*3.28084F;

}

void TWI_BME280::readSensor(void) {
    // set oversampling specs
    write8(BME280_REGISTER_CONTROLHUMID, BME280_COMMAND_OVERSAMPLING_H_X1);
    write8(BME280_REGISTER_CONTROL, BME280_COMMAND_OVERSAMPLING_P_X1 | BME280_COMMAND_OVERSAMPLING_T_X1 | BME280_COMMAND_MODE_FORCED);

    beginWireRead(BME280_REGISTER_DATA_PRESSURE, 8);    // burst read pressure/temp/hum
    // beginWireRead(BME280_REGISTER_DATA_PRESSURE, 3);    // single read
    pressure = (uint32_t)read20();

    // beginWireRead(BME280_REGISTER_DATA_TEMP, 3);
    temperature = (int32_t)read20();

    // beginWireRead(BME280_REGISTER_DATA_HUMID, 2);
    humidity = (int32_t)read16();

    compensateTemperature();
    compensatePressure();
    compensateHumidity();
}

/*
    private functions
 */

void TWI_BME280::compensateTemperature(void) {

    int32_t adc_T = temperature;
    int32_t var1, var2, t;


    var1 = ((((adc_T>>3) - ((int32_t)calibrationData.dig_T1<<1))) * ((int32_t)calibrationData.dig_T2)) >> 11;
    var2 = (((((adc_T>>4) - ((int32_t)calibrationData.dig_T1)) * ((adc_T>>4) - ((int32_t)calibrationData.dig_T1))) >> 12) * ((int32_t)calibrationData.dig_T3)) >> 14;
    t_fine = var1 + var2;

    t = ((t_fine * 5 + 128) >> 8);
    temperature = (float)t/100.0;
    temp = t;
}

void TWI_BME280::compensatePressure(void) {
    int32_t var1, var2;
    uint32_t p;
    int32_t adc_P = (int32_t)pressure;

    var1 = (((int32_t)t_fine)>>1) - (int32_t)64000;
    var2 = (((var1>>2) * (var1>>2)) >> 11 ) * ((int32_t)calibrationData.dig_P6);
    var2 = var2 + ((var1*((int32_t)calibrationData.dig_P5))<<1);
    var2 = (var2>>2)+(((int32_t)calibrationData.dig_P4)<<16);
    var1 = (((calibrationData.dig_P3 * (((var1>>2) * (var1>>2)) >> 13 )) >> 3) + ((((int32_t)calibrationData.dig_P2) * var1)>>1))>>18;
    var1 =((((32768+var1))*((int32_t)calibrationData.dig_P1))>>15);
    if (var1 == 0) {
        pressure = 0; // avoid exception caused by division by zero
        return;
    }
    p = (((uint32_t)(((int32_t)1048576)-adc_P)-(var2>>12)))*3125;
    if (p < 0x80000000) {
        p = (p << 1) / ((uint32_t)var1);
    } else {
        p = (p / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)calibrationData.dig_P9) * ((int32_t)(((p>>3) * (p>>3))>>13)))>>12;
    var2 = (((int32_t)(p>>2)) * ((int32_t)calibrationData.dig_P8))>>13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + calibrationData.dig_P7) >> 4));
    pressure = (float)p;
    press = p;
}

void TWI_BME280::compensateHumidity(void) {

    int32_t adc_H = humidity;
    
    int32_t v_x1_u32r;
    
    v_x1_u32r = (t_fine - ((int32_t)76800));
    
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)calibrationData.dig_H4) << 20) - (((int32_t)calibrationData.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)calibrationData.dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)calibrationData.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)calibrationData.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calibrationData.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    
    hum = (v_x1_u32r>>12);
    humidity = hum/1024.0;
}

void TWI_BME280::write8(uint8_t reg, uint8_t value) {
    
    Wire.beginTransmission((uint8_t)_i2caddr);
    Wire.write((uint8_t)reg);
    Wire.write((uint8_t)value);
    Wire.endTransmission();
}

uint8_t TWI_BME280::read8(uint8_t reg) {

    Wire.beginTransmission((uint8_t)_i2caddr);
    Wire.write((uint8_t)reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
    return Wire.read();
}

uint16_t TWI_BME280::read16_LE(void) {
    
    uint8_t msb, lsb;
    lsb = Wire.read();
    msb = Wire.read();
    return (msb<<8|lsb);
}

uint16_t TWI_BME280::read16(void) {
    
    uint8_t msb, lsb;
    msb = Wire.read();
    lsb = Wire.read();
    return (msb<<8|lsb);
}

bool TWI_BME280::beginWireRead(uint8_t reg, uint8_t length) {

    Wire.beginTransmission((uint8_t)_i2caddr);
    Wire.write((uint8_t)reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)length);

    return true;
}

uint32_t TWI_BME280::read20(void) {

    uint8_t msb, lsb, xlsb;
    msb = Wire.read();
    lsb = Wire.read();
    xlsb = Wire.read();
    return (uint32_t)msb<<12 | (uint16_t)lsb<<4 | (xlsb>>4 & 0x0F);
}