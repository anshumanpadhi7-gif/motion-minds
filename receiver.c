#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// nRF24L01 pins
RF24 radio(9, 10);  // CE, CSN

const byte address[6] = "HOME1";

// LED pins
const int LED1 = 2;
const int LED2 = 3;
const int LED3 = 4;
const int LED4 = 5;

// Command buffer
char command[12];

void setup() {+


  Serial.begin(9600);

  // LED pins
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  // Turn all LEDs OFF
  allLEDsOff();

  // Initialize radio
  if (!radio.begin()) {
    Serial.println("nRF24L01 NOT DETECTED");
    while (1);
  }

  radio.openReadingPipe(0, address);

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.setRetries(5, 15);

  radio.startListening();

  Serial.println("RECEIVER READY");
}

void allLEDsOff() {

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);

}

void loop() {

  if (radio.available()) {

    // Receive fixed-size command
    radio.read(command, sizeof(command));

    // Make sure the string is terminated
    command[sizeof(command) - 1] = '\0';

    Serial.print("Received: ");
    Serial.println(command);

    // Turn all LEDs OFF first
    allLEDsOff();

    // Select LED based on command

    if (strcmp(command, "LEFT") == 0) {

      digitalWrite(LED1, HIGH);
      Serial.println("LED 1 ON");

    }

    else if (strcmp(command, "RIGHT") == 0) {

      digitalWrite(LED2, HIGH);
      Serial.println("LED 2 ON");

    }

    else if (strcmp(command, "FORWARD") == 0) {

      digitalWrite(LED3, HIGH);
      Serial.println("LED 3 ON");

    }

    else if (strcmp(command, "BACK") == 0) {

      digitalWrite(LED4, HIGH);
      Serial.println("LED 4 ON");

    }

    else if (strcmp(command, "OFF") == 0) {

      allLEDsOff();
      Serial.println("ALL LEDs OFF");

    }
  }
}