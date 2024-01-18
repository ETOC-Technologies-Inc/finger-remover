// SPDX-License-Identifier: GPL-2.0-only

#define FEEDER_PIN 6
#define CUTTER_PIN 4
#define HOLLER_PIN 5

#define FEEDER_ENC_A 0
#define FEEDER_ENC_B 1

#define INPUT_ENC_SW 7
#define INPUT_ENC_A 2
#define INPUT_ENC_B 3

#define TFT_DISP_DC 8
#define TFT_DISP_RESET 9
#define TFT_DISP_CS 10

#define FEEDER_CENTER 1500
#define FEEDER_FEED_SPEED 100

#define FEEDER_CENTOFF_ADDR 0
#define FEEDER_STEPTIME_ADDR 32
#define FEEDER_MM_PER_STEP_ADDR 64

#define M_PI 3.14159265359

#include <Servo.h>
#include <Encoder.h>
#include <EEPROM.h>

#include <TFT.h> // Hardware-specific library
#include <SPI.h>

Servo g_feeder;
Servo g_cutter;
Servo g_holler;

// enc handlers
Encoder g_feeder_enc(FEEDER_ENC_A, FEEDER_ENC_B);
Encoder g_input_enc(INPUT_ENC_A, INPUT_ENC_B);

// screen
TFT g_screen = TFT(TFT_DISP_CS, TFT_DISP_DC, TFT_DISP_RESET);

// menu states
enum class EMenu: uint8_t {
	Main = 0,
	CalibrateHalfStep,
	CalibrateFeederCenter,
	CalibrateFeederMMPerStep,
	CalibrateFeederByRoller,
	Control,
	ControlFeeder,
	StartSetLen,
	StartSetNum,
	Cutting
};

EMenu curr_menu = EMenu::Main;

// op states
uint16_t target_len = 0;
uint16_t target_rep = 0;
int16_t cutter_manual_move_target = 0;
bool g_button_flag = false;
int16_t g_feeder_center_offset = 0;
uint32_t g_time_per_enc_step = 0;
int32_t g_feeder_100x_mm_per_step = 100;

void setup() {
	g_cutter.attach(CUTTER_PIN);
	g_holler.attach(HOLLER_PIN);

	g_cutter.write(180);
	g_holler.write(180);

	pinMode(INPUT_ENC_SW, INPUT_PULLUP);

	g_screen.begin();
	g_screen.background(0,0,0);
	g_screen.setTextSize(3);

	// percictency
	EEPROM.get(FEEDER_CENTOFF_ADDR, g_feeder_center_offset);
	g_feeder.attach(FEEDER_PIN);
	g_feeder.writeMicroseconds(FEEDER_CENTER + g_feeder_center_offset);
	EEPROM.get(FEEDER_STEPTIME_ADDR, g_time_per_enc_step);
	EEPROM.get(FEEDER_MM_PER_STEP_ADDR, g_feeder_100x_mm_per_step);

	Serial.begin(115200);
}

void calibrateStepTime();
void performCuttingSeq();
void cycleCutter();
void mmPerStepCalCut();
void moveToTarget(int target, int move_dir = 50);

String toString100xFloat(int32_t value) {
	return String(value / 100) + String(".") + String(abs(value) % 100);
}

void loop() {
	if (curr_menu == EMenu::CalibrateHalfStep) {
		calibrateStepTime();
		curr_menu = EMenu::Main;
		return;
	} else if (curr_menu == EMenu::CalibrateFeederByRoller) {
		int inpt = g_input_enc.read() / 2;

		int16_t result = abs(inpt) * 25;

		String text = toString100xFloat(result) + String("mm");

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			g_feeder_100x_mm_per_step = static_cast<int32_t>(static_cast<float>(result) * M_PI / 15 / 2);
			EEPROM.put(FEEDER_MM_PER_STEP_ADDR, g_feeder_100x_mm_per_step);
			curr_menu = EMenu::Main;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::CalibrateFeederMMPerStep) {
		int inpt = g_input_enc.read() / 2;

		uint16_t result = abs(inpt);

		String text = String(result) + String("mm");

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			g_feeder_100x_mm_per_step = result;
			EEPROM.put(FEEDER_MM_PER_STEP_ADDR, g_feeder_100x_mm_per_step);
			curr_menu = EMenu::Main;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;

	} else if (curr_menu == EMenu::ControlFeeder) {
		int inpt = g_input_enc.read() / 2;

		int result = inpt * g_feeder_100x_mm_per_step;

		String text = toString100xFloat(result);

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (cutter_manual_move_target != result) {
			moveToTarget(abs(cutter_manual_move_target-result), ((cutter_manual_move_target-result) > 0) ? FEEDER_FEED_SPEED : -FEEDER_FEED_SPEED);
			cutter_manual_move_target = result;
		}

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;

			curr_menu = EMenu::Control;
			cutter_manual_move_target = 0;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::Control) {
		int inpt = abs(g_input_enc.read() / 2) % 3;

		String text;

		switch (inpt) {
		case 0:
			text = "move feeder";
			break;
		case 1:
			text = "cycle cutter";
			break;
		case 2:
			text = "back";
			break;
		}

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			switch (inpt) {
			case 0:
				curr_menu = EMenu::ControlFeeder;
				g_input_enc.readAndReset();
				break;
			case 1:
				cycleCutter();
				break;
			case 2:
				curr_menu = EMenu::Main;
				g_input_enc.readAndReset();
				break;
			}
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::CalibrateFeederCenter) {
		int16_t inpt = static_cast<int16_t>(g_input_enc.read() / 2);

		int16_t result = g_feeder_center_offset + inpt;

		g_feeder.writeMicroseconds(FEEDER_CENTER + result);

		String text = String(result);

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;

			g_feeder_center_offset = result;
			EEPROM.put(FEEDER_CENTOFF_ADDR, g_feeder_center_offset);

			curr_menu = EMenu::Main;
			g_input_enc.readAndReset();
		}

		delay(10);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::StartSetLen) {
		int inpt = g_input_enc.read() / 2;

		uint16_t result = abs(inpt) * g_feeder_100x_mm_per_step;

		String text = toString100xFloat(result) + String("mm");

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			target_len = result;
			curr_menu = EMenu::StartSetNum;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::StartSetNum) {
		int inpt = g_input_enc.read() / 2;

		uint16_t result = abs(inpt);

		String text = String("x") + String(result);

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			target_rep = result;
			curr_menu = EMenu::Cutting;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::Cutting) {
		performCuttingSeq();
		curr_menu = EMenu::Main;
		return;
	} else {
		int inpt = abs(g_input_enc.read() / 2) % 6;


		String text;

		switch (inpt) {
		case 0:
			text = "start";
			break;
		case 1:
			text = "control";
			break;
		case 2:
			text = "center cal";
			break;
		case 3:
			text = "half step cal";
			break;
		case 4:
			text = "mm per step cal";
			break;
		case 5:
			text = "cal by roller";
			break;
		}

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			switch (inpt) {
			case 0:
				curr_menu = EMenu::StartSetLen;
				break;
			case 1:
				curr_menu = EMenu::Control;
				break;
			case 2:
				curr_menu = EMenu::CalibrateFeederCenter;
				break;
			case 3:
				curr_menu = EMenu::CalibrateHalfStep;
				break;
			case 4:
				curr_menu = EMenu::CalibrateFeederMMPerStep;
				mmPerStepCalCut();
				break;
			case 5:
				curr_menu = EMenu::CalibrateFeederByRoller;
				break;
			}
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	}

	delay(500);
}


void calibrateStepTime() {
	// make sure we are at the edge of a step rn
	moveToTarget(g_feeder_100x_mm_per_step, FEEDER_FEED_SPEED);

	uint32_t avrg = 0;

	for (int i=0; i < 10; i++) {
		// time moving one step
		uint32_t start = millis();
		moveToTarget(g_feeder_100x_mm_per_step, FEEDER_FEED_SPEED);
		uint32_t diff = millis() - start;
		avrg += diff;

		// do same thing but in the other direction
		start = millis();
		moveToTarget(g_feeder_100x_mm_per_step, -FEEDER_FEED_SPEED);
		diff = millis() - start;
		avrg += diff;
	}

	// calculate and save step time
	g_time_per_enc_step = avrg / 20;
	EEPROM.put(FEEDER_STEPTIME_ADDR, g_time_per_enc_step);
}

void cycleCutter() {
	g_holler.write(60);
	delay(300);
	g_cutter.write(0);
	delay(1000);
	g_cutter.write(90);
	delay(1000);
	g_cutter.write(0);
	delay(500);
	g_cutter.write(180);
	delay(600);
	g_holler.write(180);
	delay(700);
}

void moveToTarget(int target, int move_dir = 50) {
	g_feeder_enc.readAndReset();
	// if (target%g_feeder_100x_mm_per_step == 0 || g_time_per_enc_step == 0) {
		while ((target - (abs(g_feeder_enc.read() / 2) * g_feeder_100x_mm_per_step)) > 0) {
			g_feeder.write((FEEDER_CENTER + g_feeder_center_offset) + move_dir);
			// delay(10);
		}
	// } else if (g_time_per_enc_step != 0) {
	// 	if (target > g_feeder_100x_mm_per_step) {
	// 		int tmp_target = target - target % g_feeder_100x_mm_per_step;
	// 		while ((tmp_target - abs(g_feeder_enc.read() / 2) * g_feeder_100x_mm_per_step) > 0) {
	// 			g_feeder.write((FEEDER_CENTER + g_feeder_center_offset) + move_dir);
	// 			delay(10);
	// 		}
	// 		g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
	// 		target = target % g_feeder_100x_mm_per_step;
	// 	}
	// 	if (target < g_feeder_100x_mm_per_step) {
	// 		g_feeder.write((FEEDER_CENTER + g_feeder_center_offset)
	// 			+ ((move_dir > 0) ? FEEDER_FEED_SPEED : -FEEDER_FEED_SPEED));
	// 		delay(g_time_per_enc_step / 2);
	// 		g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
	// 		return;
	// 	}
	// }
	g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
}

void mmPerStepCalCut() {
	g_feeder_enc.readAndReset();
	while (abs(g_feeder_enc.read() / 2) < 100) {
		g_feeder.write((FEEDER_CENTER + g_feeder_center_offset) + FEEDER_FEED_SPEED);
	}
	g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
	cycleCutter();
}

void performCuttingSeq() {
	for (int i=0; i<target_rep; i++) {		
		String text = String("x") + String(target_rep-i);

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		moveToTarget(target_len, FEEDER_FEED_SPEED);
		cycleCutter();
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);
	}
}