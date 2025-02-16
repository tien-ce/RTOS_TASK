#include "RTOS_Task.h"
/*------------------------------------------------------------------------*/
/*------------------------Declaration-------------------------------------*/
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;      // Kích thước tối đa của thông điệp gửi qua MQTT (1024 bytes)
constexpr int16_t telemetrySendInterval = 3000U;      // Khoảng thời gian gửi dữ liệu cảm biến lên ThingsBoard (10 giây)
WiFiClient wifiClient;                            // Đối tượng WiFi để kết nối mạng
Arduino_MQTT_Client mqttClient(wifiClient);       // Đối tượng MQTT để giao tiếp với broker (ThingsBoard)
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);     // Đối tượng ThingsBoard để gửi và nhận dữ liệu qua MQTT
/*-------------------------------------------------------------------------*/

/*-----------------------------Call Back-----------------------------------*/

/*-------------------------------------------------------------------------*/
CallBack callback;

// Constructor
CallBack::CallBack(std::vector<const char*> shared_attributes, std::vector<RPC_Callback> rpc_list)
    : SHARED_ATTRIBUTES_LIST(shared_attributes), RPC_LIST(rpc_list) {}

// Thêm thuộc tính chia sẻ
void CallBack::Add_Shared_Attribute(const char* shared_attribute) {
    SHARED_ATTRIBUTES_LIST.push_back(shared_attribute);
}

// Thêm RPC Callback
void CallBack::Add_RPC(const char* rpc_name, std::function<RPC_Response(const RPC_Data&)> rpc_response) {
    RPC_LIST.emplace_back(rpc_name, rpc_response);  // Sử dụng `emplace_back()` tối ưu hơn `push_back()`
}
// Khởi tạo CallBack cho các thuộc tính chia sẻ
void CallBack::Shared_Attribute_Begin(std::function<void*(const Shared_Attribute_Data&)> Shared_callback){
    this->Shared_callback = Shared_callback;
}
// Đăng ký RPC
void SubscribeRPC(CallBack& callback){
    if (!tb.RPC_Subscribe(callback.RPC_LIST.cbegin(),callback.RPC_LIST.cend())) {  // Đăng ký nhận lệnh điều khiển từ ThingsBoard (RPC)
        Serial.println("Failed to subscribe for RPC");
        return;
    }
    const Shared_Attribute_Callback atrributes_callback(callback.Shared_callback,callback.SHARED_ATTRIBUTES_LIST.cbegin(),callback.SHARED_ATTRIBUTES_LIST.cend());
    const Attribute_Request_Callback attribute_shared_request_callback(callback.Shared_callback,callback.SHARED_ATTRIBUTES_LIST.cbegin(),callback.SHARED_ATTRIBUTES_LIST.cend());
    if(!tb.Shared_Attributes_Subscribe(atrributes_callback)){ // Đăng ký cập nhật thuộc tính chia sẻ
        Serial.println("Failed to subcribe for shared attribute updates");
        return;
    }
    if(!tb.Shared_Attributes_Request(attribute_shared_request_callback)){ // Yêu cầu Thingboard gửi lại các thuộc tính hiện tại
        Serial.println("Failed to request for shared attribute");
        return;
    }
}
// In danh sách thuộc tính và RPC
void CallBack::Print_List() {
    Serial.println("----------- SHARED_ATTRIBUTES_LIST -----------");
    for (size_t i = 0; i < SHARED_ATTRIBUTES_LIST.size(); i++) {
        Serial.print(i);
        Serial.print(" : ");
        Serial.println(SHARED_ATTRIBUTES_LIST[i]);
    }

    Serial.println("------------- RPC_LIST -------------");
    for (size_t i = 0; i < RPC_LIST.size(); i++) {
        Serial.print(i);
        Serial.print(" : ");
        Serial.println(RPC_LIST[i].Get_Name());
    }
}
/*-------------------------------------------------------------------------*/

/*--------------------------------Task-------------------------------------*/

/*-------------------------------------------------------------------------*/
void TaskBlinky(void* pvParameters){
    INIT_TASK(pvParameters);
    pinMode(Pin,OUTPUT);
    for(;;){
        digitalWrite(Pin,HIGH);
        delay(Delay);
        digitalWrite(Pin,LOW);
        delay(Delay);
    }
}
void TaskLight(void* pvParameters){
    INIT_TASK(pvParameters);
    LIGHT_VAL* light_val = (LIGHT_VAL*) pm.other;
    for(;;){
        int light = analogRead(pm.get_Pin());
        light_val->light= map(light,0,4095,0,100);
        delay(Delay);
    }
}
void TaskDht(void* pvParameters) {
    INIT_TASK(pvParameters);
    DHT_VAL *re_val = (DHT_VAL*)pm.other;
    uint8_t DHT_type = re_val->DHT_type;

    switch (DHT_type) {
        case DHT11:
        case DHT22: {
            DHT_Unified dht(Pin, DHT_type);
            dht.begin();
            for (;;) {
                sensors_event_t event;
                dht.temperature().getEvent(&event);
                re_val->temperature = event.temperature;
                dht.humidity().getEvent(&event);
                re_val->humidity = event.relative_humidity;
                delay(Delay);
            }
            break;
        }

        case Dht20: {
            DHT20 dht20;
            Wire.begin(pm.get_Pin(), pm.get_Pin() + 1);  // Khởi tạo giao tiếp I2C với cảm biến DHT20
            dht20.begin();  
            for (;;) {
                dht20.read();
                re_val->temperature = dht20.getTemperature();
                re_val->humidity = dht20.getHumidity();
                delay(Delay);
            }
            break;
        }

        default:
            break;
    }
}

void TaskTransmitUart(void* pvParameters){
    INIT_TASK(pvParameters);
    Uart_VAL transmit = *(Uart_VAL*) pm.other;
    char trans[50];
    for(;;){
        sprintf(trans,"!1:H:%.2f#!1:T:%.2f#",transmit.dht_val->humidity,transmit.dht_val->temperature);
        Serial.println(trans);
        delay(Delay);
    }
}
void TaskReceiveUart(void* pvParameters){
    INIT_TASK(pvParameters);
    pinMode(20,OUTPUT);
    for(;;){
        if(Serial.available() > 0){
            int status = Serial.read() - '0';
           
            switch (status)
            {
            case 0:
                digitalWrite(20,LOW);
                break;
            case 1:
                digitalWrite(20,HIGH);
                break;        
            default:
                break;
            }

        }
        delay(pm.get_Delay());
    }
}

void TaskPublishDataToThingsboard(void* pvParameters){
    INIT_TASK(pvParameters);
    ThingsBoard_VAL thingsboard = *((ThingsBoard_VAL*)pm.other);
    uint8_t num = 0;
    uint32_t previousDataSend = 0;
    float longti = 106.76940000  ;
    float lati = 10.90682000  ;
    for(;;){
        delay(10);
        if(WiFi.status() != WL_CONNECTED){
            WiFi.begin(thingsboard.WIFI_SSID,thingsboard.WIFI_PASSWORD); // Kết nối esp32 vào WIFI
            while (WiFi.status()!= WL_CONNECTED)
            {
                delay(500);
                Serial.print(".");
            }
            Serial.println("Connected to AP");  // Kết nối thành công
        }
        if (!tb.connected()) {  // Kiểm tra kết nối với ThingsBoard, nếu chưa kết nối thì kết nối lại
                Serial.print("Connecting to: ");
                Serial.print(thingsboard.THINGSBOARD_SERVER);
                Serial.print(" with token ");
                Serial.println(thingsboard.TOKEN);
                if (!tb.connect(thingsboard.THINGSBOARD_SERVER, thingsboard.TOKEN, thingsboard.THINGSBOARD_PORT)) {  // Kết nối đến ThingsBoard
                    Serial.println("Connected to ThingsBoard");    
                    return;   
                }
                Serial.println("Subscribing for RPC...");  
                SubscribeRPC(callback);            
        }
        if (millis() - previousDataSend > telemetrySendInterval) {  // Gửi dữ liệu cảm biến định kỳ sau mỗi 10 giây
            previousDataSend = millis();                
            float temperature = thingsboard.dht->temperature;  // Lấy dữ liệu nhiệt độ
            float humidity = thingsboard.dht->humidity;        // Lấy dữ liệu độ ẩm

            if (isnan(temperature) || isnan(humidity)) {  // Kiểm tra nếu cảm biến không trả dữ liệu hợp lệ
                Serial.println("Failed to read from DHT20 sensor!");
            } 
            else {
                Serial.print("Temperature: ");
                Serial.print(temperature);
                Serial.print(" °C, Humidity: ");
                Serial.print(humidity);
                Serial.println(" %");
                
                tb.sendTelemetryData("temperature", temperature);  // Gửi dữ liệu nhiệt độ lên ThingsBoard
                tb.sendTelemetryData("humidity", humidity);        // Gửi dữ liệu độ ẩm lên ThingsBoard
            }
            if( thingsboard.light_val->light == 0){
                Serial.println("Falied to read from light sensor");
            }
            else{
                int light = thingsboard.light_val->light;
                Serial.print("Light: ");
                Serial.println(light);
                tb.sendTelemetryData("light", light);
            }

            // Gửi các thông tin mạng WiFi lên ThingsBoard để theo dõi tình trạng kết nối
            tb.sendAttributeData("rssi", WiFi.RSSI());  
            tb.sendAttributeData("channel", WiFi.channel());
            tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
            tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
            tb.sendAttributeData("ssid", WiFi.SSID().c_str());
        }
        tb.loop();
    }
}
