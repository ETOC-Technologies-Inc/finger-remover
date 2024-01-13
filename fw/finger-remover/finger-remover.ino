
#define FEEDER_PIN 6
#define CUTTER_PIN 7

#define FEEDER_ENC_A 8
#define FEEDER_ENC_B 9
#define FEEDER_CENTER 1460

#define INPUT_ENC_SW 11
#define INPUT_ENC_A 12
#define INPUT_ENC_B 10

#define TFT_DISP_DC 2
#define TFT_DISP_RESET 3
#define TFT_DISP_CS 4

#define FEEDER_10xMM_PER_STEP 74

#include <Servo.h>
#include "encoder.hpp"

#include <TFT.h> // Hardware-specific library
#include <SPI.h>

Servo g_feeder;
Servo g_cutter;

// enc handlers
enc_util::Enc g_feeder_enc(FEEDER_ENC_A, FEEDER_ENC_B);
enc_util::Enc g_input_enc(INPUT_ENC_A, INPUT_ENC_B);

void sleepEnc(int slp) {
	for (int i=0; i<slp; +i++) {
		g_feeder_enc.update();
		g_input_enc.update();
		delay(1);
	}
}

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
	// g_feeder.writeMicroseconds(1460);

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
		int inpt = g_input_enc.pos();

		uint16_t result = abs(inpt) * FEEDER_10xMM_PER_STEP;

		String text = String(result / 10) + String("mm");

		g_screen.background(0,0,0);
		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW)) {
			target_len = result;
			curr_menu = EMenu::StartSetNum;
			g_input_enc.resetPos();
		}

		sleepEnc(30);
		return;
	} else if (curr_menu == EMenu::StartSetNum) {
		int inpt = g_input_enc.pos();

		uint16_t result = abs(inpt);

		String text = String("x") + String(result);

		g_screen.background(0,0,0);
		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW)) {
			target_rep = result;
			curr_menu = EMenu::Cutting;
			g_input_enc.resetPos();
		}

		sleepEnc(30);
		return;
	} else if (curr_menu == EMenu::Cutting) {
		performCutting();
		curr_menu = EMenu::Main;
		return;
	} else {
		int inpt = g_input_enc.pos() % 2 + 1;

		String text = (inpt == 1) ? "cal" : "start";

		g_screen.background(0,0,0);
		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW)) {
			Serial.println("press");
			curr_menu = (inpt == 1) ? EMenu::CalibrateHalfStep : EMenu::StartSetLen;
			g_input_enc.resetPos();
		}

		sleepEnc(30);

		return;
	}

	sleepEnc(500);
}


void calibrateStepTime() {

}

void performCutting() {
	for (int i=0; i<target_rep; i++) {
		g_feeder_enc.resetPos();
		g_feeder.attach(FEEDER_PIN);
		// reverse a bit
		while ((FEEDER_10xMM_PER_STEP*2 - abs(g_feeder_enc.readCont()) * FEEDER_10xMM_PER_STEP) > 0) {
			g_feeder.write(FEEDER_CENTER - 50);
		}
		sleepEnc(30);
		g_feeder_enc.resetPos();
		while ((FEEDER_10xMM_PER_STEP*2 - abs(g_feeder_enc.readCont()) * FEEDER_10xMM_PER_STEP) > 0) {
			g_feeder.write(FEEDER_CENTER + 50);
		}
		g_feeder_enc.resetPos();
		sleepEnc(30);

		while ((target_len - abs(g_feeder_enc.readCont()) * FEEDER_10xMM_PER_STEP) > 0) {
			g_feeder.write(FEEDER_CENTER + 100);
		}
		g_feeder.write(FEEDER_CENTER);
		sleepEnc(30);
		g_feeder.detach();
		g_cutter.write(0);
		sleepEnc(1000);
		g_cutter.write(180);
		sleepEnc(1000);
	}
}