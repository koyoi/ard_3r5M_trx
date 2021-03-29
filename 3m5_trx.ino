/*
    Name:       3m5_trx.ino
    Created:	2021/02/04 17:56:42
    Author:     7k4jhl-mobile\7k4jhl
*/



/* pin assign
(pro mini)
0
1
2 enc(int)
3 enc(int)
4 sw1
5 (sw2)		未使用
6 lcd
7 (nc)		led1
8 (nc)		led2
9 lcd
10 lcd
11 lcd hw-spi
12 lcd hw-spi
13 lcd hw-spi
a0 (nc)
a1 (nc)
a2 (nc)
a3 (nc)
a4 hw-i2c sda si5351, disp (ssd1306)
a5 hw-i2c scl

*/


#include "Wire.h"

// display
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(/*rotation*/U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// rot encoder
#include <Encoder.h>	// http://www.pjrc.com/teensy/td_libs_Encoder.html
// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder myEnc(3, 2);


/*
	si5351 dds
	api ref : https://github.com/etherkit/Si5351Arduino
	3.5MHz 帯の無線機
	中間周波数として 10.245MHz　＝クリスタルBPFの都合
	osc0 : 6.725付近
	osc1 : 10.245付近

*/
#include <si5351.h>
Si5351 si5351;

typedef struct {
	//            MMMkkkHHHmm
	long long f1 =  672500000LL;	// freq[Hz}*100
	long long f2 = 1024500000LL;
	char osc1_pow, osc2_pow;	// 0..3
	int correction = 0; // use the Si5351 correction sketch to find the frequency correction factor
}st_dds;
st_dds dds;

/*
	スイッチ
	pin番号
*/
const u8 SW1 = 4;
const u8 SW2 = 5;


#define NUM_SW	2
u8 sw_status[NUM_SW];	// 1：押されている
u8 sw_clicked[NUM_SW];

/*
curpos
0       osc1 power (H,M,L,-)
1..10   osc1 Freq MSB:100[MHz] .. 10[Hz]
11      osc2 power (H,M,L,-)
12..21  osc2 Freq
*/

enum {
	menu_osc1_pow,
	//	menu_osc1_freq_h10,
	//	menu_osc1_freq_h100,
	// menu_osc1_freq_1,
	menu_osc1_freq_10,
	menu_osc1_freq_100,
	menu_osc1_freq_1k,
	menu_osc1_freq_10k,
	// menu_osc1_freq_100k,
	// menu_osc1_freq_1M,
	//	menu_osc1_freq_10M,
	//	menu_osc1_freq_100M,

	menu_osc2_pow,
	//	menu_osc2_freq_h10,
	//	menu_osc2_freq_h100,
	// menu_osc2_freq_1,
	menu_osc2_freq_10,
	menu_osc2_freq_100,
	menu_osc2_freq_1k,
	menu_osc2_freq_10k,
	// menu_osc2_freq_100k,
	// menu_osc2_freq_1M,
	//	menu_osc2_freq_10M,
	//	menu_osc2_freq_100M,
	MENU_MAX
};

struct st_ui
{
	char cur_pos;
	bool cur_menu_top;	// トップメニュー / サブメニュー
	uint8_t cursor_x, cursor_y;
};

struct st_ui ui;

bool lcd_dirty;
int8_t enc_delta;

const char OSC_POWER_STR[] = { '0','1','2','3' };	// 今回、数字記号のみフォント
//const char OSC_POWER_STR[] = { 'x','L','M','H' };

int disp_count;

/*
	カーソル位置の値のインクリメント/デクリメント。限度で止まる
*/
void menu_incdec_char(char& tgt, char max, bool loop)
{
	if (enc_delta == 0)
		return;

	if (enc_delta > 0)
	{
		if (++tgt > max)
		{
			tgt = (loop ? 0 : max);
		}
	}
	else
	{
		if (--tgt < 0)
		{
			tgt = (loop ? max : 0);
		}
	}
}



void setup(void) {
	Serial.begin(19200);
	Serial.println("build:" __DATE__ "  " __TIME__ "\nstart\n");

	Serial.println("lcd init");
	u8g2.begin();	// 返り値無し（未接続を検出できない）
	u8g2.setDrawColor(2/*=xor*/);
	//	u8g2.setFont(u8g2_font_mademoiselle_mel_tn);
	u8g2.setFont(u8g2_font_t0_15_mn);
	u8g2.setFontMode(/*transparent*/true);

	Serial.println("dds init");
	si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, dds.correction);
	si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);	// 2ma ≒10dbm
	si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);

	si5351.set_freq(dds.f1, SI5351_CLK0);
	si5351.set_freq(dds.f2, SI5351_CLK1);

	pinMode(SW1, INPUT_PULLUP);
	pinMode(SW2, INPUT_PULLUP);

	ui.cur_menu_top = true;
	lcd_dirty = true;

	si5351.output_enable(SI5351_CLK0, true);

	dds.osc1_pow = 1;
	dds.osc2_pow = 0;
}



void loop(void) {
	sw_update();
	enc_delta = read_enc();

	// グラフィックテスト
	disp_count++;
	lcd_dirty = true;
	if (disp_count >= 100)
	{
		disp_count = 0;
	}


	if (sw_isClicked(0))
	{
		ui.cur_menu_top ^= 1;
		lcd_dirty = true;
		Serial.println("sw1 click");
	}

	if (sw_isClicked(1))
	{
		Serial.println("sw2 click");
	}

	if (enc_delta != 0)
	{
		change_values();
	}

	debug_serial_print();

	if (enc_delta != 0 || lcd_dirty)
	{
		disp_update();
		lcd_dirty = false;
	}
}



void change_values()
{
	if (ui.cur_menu_top)
	{
		// カーソル位置更新
		menu_incdec_char(ui.cur_pos, MENU_MAX - 1, true);	// やってること一緒だけど、関数使いまわしはよくないかも
	}
	else
	{
		// サブメニュー
		// 値変更
		switch (ui.cur_pos)
		{
		case(menu_osc1_pow):
			menu_incdec_char(dds.osc1_pow, SI5351_DRIVE_8MA, false);
			if (dds.osc1_pow == 0) {
				si5351.output_enable(SI5351_CLK0, false);
			}
			else
			{
				si5351.output_enable(SI5351_CLK0, true);
				si5351.drive_strength(SI5351_CLK0, (si5351_drive)(dds.osc1_pow - 1));
			}
			break;

		case(menu_osc2_pow):
			menu_incdec_char(dds.osc2_pow, SI5351_DRIVE_8MA, false);
			if (dds.osc2_pow == 0) {
				si5351.output_enable(SI5351_CLK1, false);
			}
			else
			{
				si5351.output_enable(SI5351_CLK1, true);
				si5351.drive_strength(SI5351_CLK1, (si5351_drive)(dds.osc2_pow - 1));
			}
			break;
		case(menu_osc1_freq_100):
		case(menu_osc1_freq_10):
			/* fall through */
		case(menu_osc1_freq_10k):
		case(menu_osc1_freq_1k):
			/* fall through */
			if (enc_delta != 0) {
				change_freq(&(dds.f1), (ui.cur_pos - menu_osc1_freq_10)+1);
				si5351.set_freq(dds.f1, SI5351_CLK0);
			}
			break;
		case(menu_osc2_freq_100):
		case(menu_osc2_freq_10):
			/* fall through */
		case(menu_osc2_freq_10k):
		case(menu_osc2_freq_1k):
			if (enc_delta != 0) {
				change_freq(&(dds.f2), (ui.cur_pos - menu_osc2_freq_10)+1);
				si5351.set_freq(dds.f2, SI5351_CLK1);
			}
			break;
		default:
			// assert("cur pos illegal");
			break;
		}
	}

	// disp カーソル位置
#define LH 14
	char disp_dots = 2;
	switch (ui.cur_pos)
	{
	case(menu_osc1_pow):
		ui.cursor_x = 0;
		ui.cursor_y = LH;
		break;

	case(menu_osc2_pow):
		ui.cursor_x = 0;
		ui.cursor_y = LH*2;
		break;
	case(menu_osc1_freq_100):
	case(menu_osc1_freq_10):
		disp_dots += 1;
		/* fall through */
	case(menu_osc1_freq_10k):
	case(menu_osc1_freq_1k):
		ui.cursor_y = LH;
		ui.cursor_x = 8 * (5 - (ui.cur_pos - menu_osc1_freq_10k) + disp_dots);
		break;
	case(menu_osc2_freq_100):
	case(menu_osc2_freq_10):
		disp_dots += 1;
		/* fall through */
	case(menu_osc2_freq_10k):
	case(menu_osc2_freq_1k):
		ui.cursor_y = LH*2;
		ui.cursor_x = 8 * (5 - (ui.cur_pos - menu_osc2_freq_10k) + disp_dots);
		break;
	}
	Serial.print("cur(x,y)=");
	Serial.print(ui.cursor_x);
	Serial.print(", ");
	Serial.println(ui.cursor_y);
}


/*
	変化量はこの中で end_delta を取ってくる
	args：
	 対象osc,
	 何桁目？ 0: 1Hz
	 dds自体は10mHz分解のようだが
*/
void change_freq(long long int* f, u8 digit)
{
	long long int f_temp;

	if (enc_delta == 0)
		return;

	long long int fact = 100;
	for (uint8_t i = 0; i < digit; i++) {
		fact *= 10;
	}

	f_temp = *f + fact * enc_delta;
	*f = f_temp;
	return;
}


void debug_serial_print()
{
	char str[40];
	/*
	{
		static long enc_old;
		if (enc_old != myEnc.read() )
		{
			enc_old = myEnc.read();
			Serial.print("enc:");
			Serial.println(enc_old);
		}

	}
	*/

	if (enc_delta == 0)
		return;

	Serial.print("cur:");
	itoa(ui.cur_pos, str, DEC);
	Serial.println(str);

	//	Serial.print("f1:");
	//	ll2str_cm(&f1, str);
	//	Serial.println(str);

}




#define SW_CHATTERING_MASK	0b00000111
const u8 sw_pins[NUM_SW] = { SW1, SW2 };
void sw_update() {
	static u8 sw_debouns[NUM_SW];

	for (u8 i = 0; i < NUM_SW; i++)
	{
		sw_debouns[i] = ((sw_debouns[i] << 1) | (digitalRead(sw_pins[i]) & 1));
		u8 _sw_status_old = sw_status[i];
		if ((sw_debouns[i] & SW_CHATTERING_MASK) == 0x0) { sw_status[i] = 1; }
		else if ((sw_debouns[i] & SW_CHATTERING_MASK) == SW_CHATTERING_MASK) { sw_status[i] = 0; }
		if (_sw_status_old == 0 && sw_status[i] == 1) { sw_clicked[i] = 1; }
	}
}


u8 sw_isClicked(u8 swnum)
{
	u8 rv = sw_clicked[swnum];
	sw_clicked[swnum] = 0;
	return rv;
}

static char curpos_treat_points(char x)
{
	if (x > 7) { x++; }	// .xx Hz
	if (x > 1) { x++; } // 100k - 1Hz 
	return x;
}

static void disp_update() {
	char str[2][16];
	char str_test[5];
	static char lh;	// line height

	str[0][0] = OSC_POWER_STR[dds.osc1_pow];
	str[0][1] = ' ';
	ll2str_cm(dds.f1, &str[0][2]);

	str[1][0] = OSC_POWER_STR[dds.osc2_pow];
	str[1][1] = ' ';
	ll2str_cm(dds.f2, &str[1][2]);

	lh = 14;	// line height

	sprintf(str_test, "%3d", disp_count);

	u8g2.clearBuffer();
	u8g2.firstPage();
	do{
		u8g2.drawFrame(0, 0, 128, 64);
//		u8g2.setFont(u8g2_font_mademoiselle_mel_tn);
//		u8g2.setFont(u8g2_font_t0_15_mn);
		u8g2.drawStr(0, lh, str[0]);
		u8g2.drawStr(0, 2 * lh, str[1]);
		
		// カーソル
		if (ui.cur_menu_top)
		{
			u8g2.drawHLine(ui.cursor_x, ui.cursor_y+1, 8);
		}
		else
		{
			u8g2.drawBox(ui.cursor_x, ui.cursor_y - lh+2, 8, lh-1);
		}

		u8g2.drawStr(0, 4 * lh, str_test);
	} while (u8g2.nextPage());
}



int8_t read_enc() {
	static long oldPosition = 0;

	long newPosition = myEnc.read() / 4;
	int8_t delta = newPosition - oldPosition;
	oldPosition = newPosition;
	return delta;
}


void assert(char* msg)
{
	Serial.println(msg);
	// while(1){;}
}


const int buff_size = 16;

void ll2str_cm(long long int& num, char* buf) {

	char s_temp[16];

	long int t1 = num / 100;

	sprintf(s_temp, "%9ld", t1);

	// xxx,xxx,xxx.xx

	*buf = s_temp[0];
	*(buf + 1) = s_temp[1];
	*(buf + 2) = s_temp[2];
	*(buf + 3) = (s_temp[2] == ' ') ? ' ' : '.';
	*(buf + 4) = s_temp[3];
	*(buf + 5) = s_temp[4];
	*(buf + 6) = s_temp[5];
	*(buf + 7) = (s_temp[5] == ' ') ? ' ' : ',';
	*(buf + 8) = s_temp[6];
	*(buf + 9) = s_temp[7];
	*(buf + 10) = s_temp[8];
	*(buf + 11) = '\0';
	/*
	*(buf + 11) = (s_temp[8] == ' ') ? ' ' : '.';
	*(buf + 12) = s_temp[9];
	*(buf + 13) = s_temp[10];
	*(buf + 14) = '\0';
	*/
}
