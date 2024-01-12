
#define FEEDER_PIN 6
#define CUTTER_PIN 7

#define FEEDER_ENC_A 8
#define FEEDER_ENC_B 9

#define INPUT_ENC_SW 11
#define INPUT_ENC_A 12
#define INPUT_ENC_B 13

#define FEEDER_MM_PER_STEP 7.5

#include <Servo.h>

Servo g_feeder;
Servo g_cutter;

class Enc {
public:
	Enc(int pin_a, int pin_b):
		counter(0),
		old_AB(3),
		encval(0),
		pinA(pin_a), pinB(pin_b),
		enc_states({0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}) {
		pinMode(pinA, INPUT_PULLUP);
		pinMode(pinB, INPUT_PULLUP);
	}

	int read() {
		old_AB <<=2;  // Remember previous state  

		if (digitalRead(pinA)) old_AB |= 0x02; // Add current state of pin A
		if (digitalRead(pinB)) old_AB |= 0x01; // Add current state of pin B

		encval += enc_states[( old_AB & 0x0f )];

		// Update counter if encoder has rotated a full indent, that is at least 4 steps
		if( encval > 3 ) {        // Four steps forward
			encval = 0;
			return 1;              // Increase counter
		}
		else if( encval < -3 ) {  // Four steps backwards
			encval = 0;
			return -1;               // Decrease counter
		}

		return 0;
	}

	int readCont() {
		counter += read();
		return counter;
	}

private:
	int counter;
	uint8_t old_AB;
	int8_t encval;
	int pinA;
	int pinB;

	const int8_t enc_states[16];
};

Enc g_feeder_enc(FEEDER_ENC_A, FEEDER_ENC_B);
Enc g_input_enc(INPUT_ENC_A, INPUT_ENC_B);

void setup() {
	g_feeder.attach(FEEDER_PIN);
	g_cutter.attach(CUTTER_PIN);

	g_cutter.write(180);
	g_feeder.writeMicroseconds(1500);

	pinMode(INPUT_ENC_SW, INPUT_PULLUP);

	Serial.begin(115200); // Change to 9600 for Nano, 115200 for ESP32
}

int read_encoders(int pin_a, int pin_b);

void loop() {
	int inpt = g_input_enc.readCont();

	g_feeder.writeMicroseconds(1500 + 10*inpt);

	if (inpt == 0) {
		if (g_feeder.attached()) {
			g_feeder.detach();
		}
	} else {
		if (!g_feeder.attached()) {
			g_feeder.attach(FEEDER_PIN);
		}
	}

	Serial.print(g_feeder_enc.readCont());
	Serial.print(", ");
	Serial.println(inpt);
}