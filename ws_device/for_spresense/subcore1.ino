#define SUBCORE
#include <MP.h>

int sensor_value = 0;
float temperature = 0.0;

// for MP send and recv function
int8_t msgid = 1;
uint32_t msgdata = 0;

const int PIN_ANALOG_INPUT = 0;

void setup() {
    Serial.begin(115200);

    MP.begin();
    Serial.println("Subcore is ready.");
}

void loop() {
    sensor_value = analogRead(PIN_ANALOG_INPUT);
    temperature = ((sensor_value * 5.0 / 1023.0) * 1000.0 - 500.0) / 10.0;
    msgdata = (uint32_t)(temperature * 10.0); 

    Serial.print("Temperature: ");
    Serial.println(temperature);

    MP.Send(msgid, msgdata);
    delay(10000);
}
