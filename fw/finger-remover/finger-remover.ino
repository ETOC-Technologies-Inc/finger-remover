
#define FEEDER_PIN 6
#define CUTTER_PIN 5

#define FEEDER_ENC_A 0
#define FEEDER_ENC_B 1

#define INPUT_ENC_SW 7
#define INPUT_ENC_A 2
#define INPUT_ENC_B 3

#define TFT_DISP_DC 8
#define TFT_DISP_RESET 9
#define TFT_DISP_CS 10

#define FEEDER_10xMM_PER_STEP 74
#define FEEDER_CENTER 1500
#define FEEDER_FEED_SPEED 100

#define FEEDER_CENTOFF_ADDR 0
#define FEEDER_STEPTIME_ADDR 32

#include <Servo.h>
#include <Encoder.h>
#include <EEPROM.h>

#include <TFT.h> // Hardware-specific library
#include <SPI.h>

Servo g_feeder;
Servo g_cutter;

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
	Control,
	ControlFeeder,
	StartSetLen,
	StartSetNum,
	Cutting
};

EMenu curr_menu = EMenu::Main;

// op states
uint16_t target_len_10x = 0;
uint16_t target_rep = 0;
bool g_button_flag = false;
int16_t g_feeder_center_offset = 0;
uint32_t g_time_per_enc_step = 0;

void setup() {
	g_feeder.attach(FEEDER_PIN);
	g_cutter.attach(CUTTER_PIN);

	g_cutter.write(180);

	pinMode(INPUT_ENC_SW, INPUT_PULLUP);

	g_screen.begin();
	g_screen.background(0,0,0);
	g_screen.setTextSize(3);

	// percictency
	EEPROM.get(FEEDER_CENTOFF_ADDR, g_feeder_center_offset);
	g_feeder.writeMicroseconds(FEEDER_CENTER + g_feeder_center_offset);
	EEPROM.get(FEEDER_STEPTIME_ADDR, g_time_per_enc_step);

	Serial.begin(115200);
}

void calibrateStepTime();
void performCuttingSeq();
void cycleCutter();
void moveToTarget(int target, int move_dir = 50);

void loop() {
	if (curr_menu == EMenu::CalibrateHalfStep) {
		calibrateStepTime();
		curr_menu = EMenu::Main;
		return;
	} else if (curr_menu == EMenu::ControlFeeder) {
		int inpt = g_input_enc.read();

		int result = inpt * FEEDER_10xMM_PER_STEP;

		String text = String(inpt);

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (target_len_10x != result) {
			target_len_10x = result;
			moveToTarget(result, (result > 0) ? 50 : -50);
		}

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;

			curr_menu = EMenu::Control;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::Control) {
		int inpt = g_input_enc.read() % 3;

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
				break;
			case 1:
				cycleCutter();
				break;
			case 2:
				curr_menu = EMenu::Main;
				break;
			}
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::CalibrateFeederCenter) {
		int16_t inpt = static_cast<int16_t>(g_input_enc.read());

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
		int inpt = g_input_enc.read();

		uint16_t result = abs(inpt) * 50;

		String text = String(result / 10) + String("mm");

		g_screen.stroke(255, 255, 255);
		g_screen.noFill();

		g_screen.textWrap(text.c_str(), 5, 20);

		if (!digitalRead(INPUT_ENC_SW) && !g_button_flag) {
			g_button_flag = true;
		} else if (digitalRead(INPUT_ENC_SW) && g_button_flag) {
			g_button_flag = false;
			target_len_10x = result;
			curr_menu = EMenu::StartSetNum;
			g_input_enc.readAndReset();
		}

		delay(100);
		g_screen.stroke(0,0,0);
		g_screen.textWrap(text.c_str(), 5, 20);

		return;
	} else if (curr_menu == EMenu::StartSetNum) {
		int inpt = g_input_enc.read();

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
		int inpt = g_input_enc.read() % 4;

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
	moveToTarget(FEEDER_10xMM_PER_STEP, FEEDER_FEED_SPEED);

	uint32_t avrg = 0;

	for (int i=0; i < 10; i++) {
		// time moving one step
		uint32_t start = millis();
		moveToTarget(FEEDER_10xMM_PER_STEP, FEEDER_FEED_SPEED);
		uint32_t diff = millis() - start;
		avrg += diff;

		// do same thing but in the other direction
		start = millis();
		moveToTarget(FEEDER_10xMM_PER_STEP, -FEEDER_FEED_SPEED);
		diff = millis() - start;
		avrg += diff;
	}

	// calculate and save step time
	g_time_per_enc_step = avrg / 20;
	EEPROM.put(FEEDER_STEPTIME_ADDR, g_time_per_enc_step);
}

void cycleCutter() {
	g_cutter.write(0);
	delay(1000);
	g_cutter.write(180);
	delay(1000);
}

void moveToTarget(int target, int move_dir = 50) {
	g_feeder_enc.readAndReset();
	if (target%FEEDER_10xMM_PER_STEP == 0 || g_time_per_enc_step == 0) {
		while ((target - abs(g_feeder_enc.read()) * FEEDER_10xMM_PER_STEP) > 0) {
			g_feeder.write((FEEDER_CENTER + g_feeder_center_offset) + move_dir);
			delay(10);
		}
	} else if (g_time_per_enc_step != 0) {
		if (target > FEEDER_10xMM_PER_STEP) {
			int tmp_target = target - target % FEEDER_10xMM_PER_STEP;
			while ((tmp_target - abs(g_feeder_enc.read()) * FEEDER_10xMM_PER_STEP) > 0) {
				g_feeder.write((FEEDER_CENTER + g_feeder_center_offset) + move_dir);
				delay(10);
			}
			g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
			target = target % FEEDER_10xMM_PER_STEP;
		}
		if (target < FEEDER_10xMM_PER_STEP) {
			g_feeder.write((FEEDER_CENTER + g_feeder_center_offset)
				+ ((move_dir > 0) ? FEEDER_FEED_SPEED : -FEEDER_FEED_SPEED));
			delay(g_time_per_enc_step / 2);
			g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
			return;
		}
	}
	g_feeder.write((FEEDER_CENTER + g_feeder_center_offset));
}

void performCuttingSeq() {
	for (int i=0; i<target_rep; i++) {
		// reverse a bit and go back
		moveToTarget(FEEDER_10xMM_PER_STEP*2, -50);
		moveToTarget(FEEDER_10xMM_PER_STEP*2, 50);

		// move to target and perform the cut
		moveToTarget(target_len_10x, FEEDER_FEED_SPEED);
		cycleCutter();
	}
}