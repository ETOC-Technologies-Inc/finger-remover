
#define FEEDER_PIN 6
#define CUTTER_PIN 7

#define FEEDER_ENC_A 8
#define FEEDER_ENC_B 9

#define INPUT_ENC_SW 11
#define INPUT_ENC_A 12
#define INPUT_ENC_B 13

#define TFT_DISP_DC 0
#define TFT_DISP_RESET 1
#define TFT_DISP_CS 2

#define FEEDER_10xMM_PER_STEP 74

#include <Servo.h>
#include "encoder.hpp"

#include <TFT.h> // Hardware-specific library
#include <SPI.h>

Servo g_feeder;
Servo g_cutter;

enc_util::Enc g_feeder_enc(FEEDER_ENC_A, FEEDER_ENC_B);
enc_util::Enc g_input_enc(INPUT_ENC_A, INPUT_ENC_B);

TFT g_screen = TFT(TFT_DISP_CS, TFT_DISP_DC, TFT_DISP_RESET);

enum class EMenu: uint8_t {
	Main = 0,
	CalibrateHalfStep,
	StartSetLen,
	StartSetNum,
	Cutting
};


EMenu curr_menu = EMenu::Main;

uint16_t target_len = 0;
uint16_t target_rep = 0;

void setup() {
	// g_feeder.attach(FEEDER_PIN);
	g_cutter.attach(CUTTER_PIN);

	g_cutter.write(180);
	// g_feeder.writeMicroseconds(1500);

	pinMode(INPUT_ENC_SW, INPUT_PULLUP);

	g_screen.begin();
	g_screen.background(0,0,0);
	g_screen.setTextSize(3);

	Serial.begin(115200); // Change to 9600 for Nano, 115200 for ESP32
}

void calibrateStepTime();
void performCutting();

void loop() {
	if (curr_menu == EMenu::CalibrateHalfStep) {
		calibrateStepTime();
		curr_menu = EMenu::Main;
		return;
	} else if (curr_menu == EMenu::StartSetLen) {
		int inpt = g_input_enc.readCont();

		uint16_t result = abs(inpt) * FEEDER_10xMM_PER_STEP / 10;

		String text = String(result) + String("mm");

		g_screen.background(0,0,0);
		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 0, 20);

		if (!digitalRead(INPUT_ENC_SW)) {
			target_rep = result;
			curr_menu = EMenu::StartSetNum;
			g_input_enc.resetPos();
		}

		delay(100);
		return;
	} else if (curr_menu == EMenu::StartSetNum) {
		int inpt = g_input_enc.readCont();

		uint16_t result = abs(inpt);

		String text = String("x") + String(result);

		g_screen.background(0,0,0);
		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 0, 20);

		if (!digitalRead(INPUT_ENC_SW)) {
			target_len = result;
			curr_menu = EMenu::Cutting;
			g_input_enc.resetPos();
		}

		delay(100);
		return;
	} else if (curr_menu == EMenu::Cutting) {
		performCutting();
		curr_menu = EMenu::Main;
		return;
	} else {
		int inpt = g_input_enc.readCont() % 2 + 1;

		String text = (inpt == 1) ? "cal" : "start";

		g_screen.background(0,0,0);
		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 0, 20);

		if (!digitalRead(INPUT_ENC_SW)) {
			curr_menu = (EMenu)inpt;
			g_input_enc.resetPos();
		}

		delay(100);
		return;
	}

	// int inpt = g_input_enc.readCont();

	// g_feeder.writeMicroseconds(1500 + 10*inpt);

	// if (inpt == 0) {
	// 	if (g_feeder.attached()) {
	// 		g_feeder.detach();
	// 	}
	// } else {
	// 	if (!g_feeder.attached()) {
	// 		g_feeder.attach(FEEDER_PIN);
	// 	}
	// }

	// Serial.print(g_feeder_enc.readCont());
	// Serial.print(", ");
	// Serial.println(inpt);
}


void calibrateStepTime() {

}

void performCutting() {

}