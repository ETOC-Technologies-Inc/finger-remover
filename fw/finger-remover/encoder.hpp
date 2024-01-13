#ifndef ENCODER_HPP
#define ENCODER_HPP

namespace enc_util {

constexpr static const int8_t g_enc_states[16] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};

class Enc {
public:
	Enc(int pin_a, int pin_b):
		counter(0),
		old_AB(3),
		encval(0),
		pinA(pin_a),
		pinB(pin_b)
	{
		pinMode(pinA, INPUT_PULLUP);
		pinMode(pinB, INPUT_PULLUP);
	}

	int read() {
		old_AB <<=2;  // Remember previous state  

		if (digitalRead(pinA)) old_AB |= 0x02; // Add current state of pin A
		if (digitalRead(pinB)) old_AB |= 0x01; // Add current state of pin B

		encval += g_enc_states[( old_AB & 0x0f )];

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

	void update() {
		counter += read();
	}

	int pos() const {
		return counter;
	}

	int getPinA() const {
		return pinA;
	}

	int getPinB() const {
		return pinB;
	}

	void resetPos() {
		counter = 0;
	}

private:
	int counter;
	uint8_t old_AB;
	int8_t encval;
	int pinA;
	int pinB;
};

} // namespace enc_util

#endif // #ifndef ENCODER_HPP