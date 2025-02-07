#include "RTOS_Task.h"
/*------------------------------------------*/
/*--------------Task------------------------*/
/*------------------------------------------*/
void TaskBlinky(void* pvParameters){
    INIT_TASK(pvParameters);
    for(;;){
        pinMode(Pin,OUTPUT);
        digitalWrite(Pin,HIGH);
        delay(Delay);
        digitalWrite(Pin,LOW);
        delay(Delay);
    }
}
void TaskDht(void* pvParameters){
    INIT_TASK(pvParameters);
    DHT_VAL *re_val = (DHT_VAL*)pm.other;
    uint8_t DHT_type = re_val->DHT_type;
    DHT_Unified dht(Pin, DHT_type);
    dht.begin();
    for(;;){
        sensors_event_t event;
        dht.temperature().getEvent(&event);
        // Serial.print("TEMP: ");
        // Serial.println(re_val->temperature);
        re_val->temperature = event.temperature;
        // Serial.print("HUM: ");
        // Serial.println(re_val->humidity);
        dht.humidity().getEvent(&event);
        re_val->humidity = event.relative_humidity;
        char trans[50];
        delay(Delay);
    }
}
void TaskUart(void* pvParameters){
    INIT_TASK(pvParameters);
    Uart_VAL transmit = *(Uart_VAL*) pm.other;
    char trans[50];
    for(;;){
        sprintf(trans,"!1:H:%.2f#!1:T:%.2f#",transmit.dht_val->humidity,transmit.dht_val->temperature);
        Serial.println(trans);
        delay(Delay);
    }
}