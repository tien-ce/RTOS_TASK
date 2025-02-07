#ifndef TASK_H
#define TASH_H
#include <Arduino.h>
#include <DHT.h>
#include <DHT_U.h>
#include "string.h"
/*------------------------Macro-----------------------------------*/
#define CreateTask(name_task,stack,Pin,Delay,other) \
    Parameter pm_##name_task(Pin,Delay,other); \
    xTaskCreate(Task##name_task, #name_task,stack,(void*) &pm_##name_task,2,NULL);
/*-------------------------Struct-----------------------------------*/    
struct Uart_VAL{
    char* data;
    int num ;

};
struct DHT_VAL{
    uint8_t DHT_type;
    float temperature;
    float humidity;
    DHT_VAL(uint8_t DHT_type){
        this-> DHT_type = DHT_type; 
        this->temperature = 0;
        this->humidity = 0;
    }
};
class Parameter{
    private:
        uint8_t Pin;
        uint32_t Task_Delay;
    public:
        void* other;
        Parameter(uint8_t Pin = 255,uint32_t delay = 1000,void* other = NULL){
            this->Pin = Pin;
            this->Task_Delay = delay;
            this->other = other;
        }
        uint8_t get_Pin(){
            return this->Pin;
        }
        uint32_t get_Delay(){
            return this->Task_Delay;
        }
};
void TaskBlinky(void* pvParameters);
void TaskDht(void* pvParameters);
void TaskUart(void* pvParameters);
#endif