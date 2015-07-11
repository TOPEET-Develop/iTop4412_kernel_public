/*
 * linux/sound/soc/codecs/wm8994-path.c
 * 
 * Handle codec path ralated settings
 */
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/delay.h>
#include "wm8994.h"

extern int wm8994_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value);
extern unsigned int wm8994_read(struct snd_soc_codec *codec,
				unsigned int reg);
//#define WENPIN_DEBUG  
//#define DEBUG 
#ifdef DEBUG
#define dprintk(x...) 	printk(x)
#else
#define dprintk(x...)
#endif

#define HIGH_VOLUME

/*
 * Dump registers of wm8994
 */
#ifdef DEBUG
static void wm8994_dump(struct snd_soc_codec *codec)
{
	unsigned int i;

	dprintk("[zsb] wm8994 dump start!\n");
	for (i = 0; i <= 0x700; i++) {
		printk("reg: 0x%03x  data: 0x%04x", i, wm8994_read(codec, i));
		if (i % 4 == 3)
			printk("\n");
		else
			printk("\t");
	}
}
#endif

int set_playback_path(struct snd_soc_codec *codec, u8 playback_path,
		u8 capture_path, enum snd_soc_bias_level level)
{
	int ret = 0;

	dprintk ("%s() :playbackpath = %d,capture_path %d\n", __func__,playback_path,capture_path);

	switch(playback_path) {
	case PLAYBACK_REC_NORMAL:
	case PLAYBACK_REC_RING:
		switch(capture_path) {
		case CAPTURE_MAIN_MIC_NORMAL:
		case CAPTURE_HAND_MIC_NORMAL:
		default:
			break;
		}
		break;
	case PLAYBACK_SPK_NORMAL:
	case PLAYBACK_SPK_RING:			
		switch(capture_path) {
		case CAPTURE_MAIN_MIC_NORMAL:
		case CAPTURE_HAND_MIC_NORMAL:
			break;
		default:
			//path
			wm8994_write(codec, 0x0601,  0x0001);	//aif1.1dacL --> dac1L
			wm8994_write(codec, 0x0602,  0x0001);	//aif1.1dacR --> dac1R
			wm8994_write(codec, 0x0036,  0x0003);	//dac1L --> spkmixL, dac1R --> spkmixR
			wm8994_write(codec, 0x0024,  0x0011);	//spkmixL --> spkoutL, spkmixR --> spkoutR
			//gain
#ifdef HIGH_VOLUME
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0402,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0403,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x0022,  0x0000);	//spkmixL volume
			wm8994_write(codec, 0x0023,  0x0000);	//spkmixR volume
			wm8994_write(codec, 0x0026,  0x017F);	//spkoutL volume
			wm8994_write(codec, 0x0027,  0x017F);	//spkoutR volume
			wm8994_write(codec, 0x0025,  0x002D);	//spkoutL, 2.37xboost
#else
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0402,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0403,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x0022,  0x0000);	//spkmixL volume
			wm8994_write(codec, 0x0023,  0x0000);	//spkmixR volume
			wm8994_write(codec, 0x0026,  0x017C);	//spkoutL volume
			wm8994_write(codec, 0x0027,  0x017C);	//spkoutR volume
			wm8994_write(codec, 0x0025,  0x0024);	//spkoutL,R boost
#endif
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			wm8994_write(codec, 0x0208,  0x001a);	//aif1dspclk ena, sysdspclk ena, sysclk src: aif1
			//power
			wm8994_write(codec, 0x0001,  0x3003);	//spkL,R ena
			wm8994_write(codec, 0x0002,  0x0000);	//input disable
			wm8994_write(codec, 0x0003,  0x0300);	//spkLvol, spkRvol ena
			/* wenpin.cui: Playback, DONOT operate ADC */
		//	wm8994_write(codec, 0x0004,  0x0000);	//adc disable wenpin.cui
			wm8994_write(codec, 0x0005,  0x0303);	//aif1dacL, aif1dacR ena, dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0420,  0x0000);	//aif1dac un-mute
			break;
		}
		break;
	case PLAYBACK_HP_NORMAL:
	case PLAYBACK_HP_RING:
		switch(capture_path) {
		case CAPTURE_MAIN_MIC_NORMAL:
		case CAPTURE_HAND_MIC_NORMAL:
			break;
		default:
			//path
			wm8994_write(codec, 0x002D,  0x0100);	//dac1L --> hpout1Lvol
			wm8994_write(codec, 0x002E,  0x0100);	//dac1R --> hpout1Rvol
			wm8994_write(codec, 0x0601,  0x0001);	//aif1dacL --> dac1L
			wm8994_write(codec, 0x0602,  0x0001);	//aif1dacR --> dac1R
			wm8994_write(codec, 0x0036,  0x0000);	//dac1L !--> spkmixL, dac1R !--> spkmixR
			//gain
#ifdef HIGH_VOLUME
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0402,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0403,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x001C,  0x01FF);	//hpout1 L volume, maximum
			wm8994_write(codec, 0x001D,  0x01FF);	//hpout1 R volume, maximum
			wm8994_write(codec, 0x0020,  0x01F9);	//mixout L volume, not useful
			wm8994_write(codec, 0x0021,  0x01F9);	//mixout R volume, not useful
#else
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0402,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0403,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x001C,  0x01ED);	//hpout1 L volume
			wm8994_write(codec, 0x001D,  0x01ED);	//hpout1 R volume
			wm8994_write(codec, 0x0020,  0x01F9);	//mixout L volume, not useful
			wm8994_write(codec, 0x0021,  0x01F9);	//mixout R volume, not useful
#endif
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			wm8994_write(codec, 0x0208,  0x001a);	//aif1dspclk ena, sysdspclk ena, sysclk src: aif1
			//HP_misc
			wm8994_write(codec, 0x0051,  0x0005); 
			wm8994_write(codec, 0x0060,  0x0022);
			wm8994_write(codec, 0x004C,  0x9F25);
			wm8994_write(codec, 0x0055,  0x0541);
			wm8994_write(codec, 0x0055,  0x0401);
			wm8994_write(codec, 0x0054,  0x0303);
			wm8994_write(codec, 0x0057,  0xFBFB);
			wm8994_write(codec, 0x0054,  0x000F);
			wm8994_write(codec, 0x0060,  0x00EE);
			//power
			wm8994_write(codec, 0x0001,  0x0303);	//hpout1L,R ena
			wm8994_write(codec, 0x0002,  0x0000);	//input disable wenpin.cui
			wm8994_write(codec, 0x0003,  0x0000);	//spkLvol, spkRvol !ena
			/* wenpin.cui: Playback, DONOT operate ADC*/
//			wm8994_write(codec, 0x0004,  0x0000);	//adc disable 
			wm8994_write(codec, 0x0005,  0x0303);	//aif1dacL, aif1dacR ena, dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0420,  0x0000);	//aif1dac un-mute
			break;
		}
		break;
	case PLAYBACK_SPK_HP_NORMAL:
	case PLAYBACK_SPK_HP_RING:
		switch(capture_path) {
		case CAPTURE_MAIN_MIC_NORMAL:
		case CAPTURE_HAND_MIC_NORMAL:
			break;
		default:
			//path
			wm8994_write(codec, 0x0601,  0x0001);	//aif1.1dacL --> dac1L
			wm8994_write(codec, 0x0602,  0x0001);	//aif1.1dacR --> dac1R
			wm8994_write(codec, 0x0036,  0x0003);	//dac1L --> spkmixL, dac1R --> spkmixR
			wm8994_write(codec, 0x0024,  0x0011);	//spkmixL --> spkoutL, spkmixR --> spkoutR
			wm8994_write(codec, 0x002D,  0x0100);	//dac1L --> hpout1Lvol
			wm8994_write(codec, 0x002E,  0x0100);	//dac1R --> hpout1Rvol
			//gain
#ifdef HIGH_VOLUME
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0402,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0403,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x0022,  0x0000);	//spkmixL volume
			wm8994_write(codec, 0x0023,  0x0000);	//spkmixR volume
			wm8994_write(codec, 0x0026,  0x017F);	//spkoutL volume
			wm8994_write(codec, 0x0027,  0x017F);	//spkoutR volume
			wm8994_write(codec, 0x0025,  0x002D);	//spkoutL,R boost
			wm8994_write(codec, 0x001C,  0x01FF);	//hpout1 L volume
			wm8994_write(codec, 0x001D,  0x01FF);	//hpout1 R volume
			wm8994_write(codec, 0x0020,  0x01F9);	//mixout L volume, not useful
			wm8994_write(codec, 0x0021,  0x01F9);	//mixout R volume, not useful
#else
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0402,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0403,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x0022,  0x0000);	//spkmixL volume
			wm8994_write(codec, 0x0023,  0x0000);	//spkmixR volume
			wm8994_write(codec, 0x0026,  0x017C);	//spkoutL volume
			wm8994_write(codec, 0x0027,  0x017C);	//spkoutR volume
			wm8994_write(codec, 0x0025,  0x0024);	//spkoutL,R boost
			wm8994_write(codec, 0x001C,  0x01ED);	//hpout1 L volume
			wm8994_write(codec, 0x001D,  0x01ED);	//hpout1 R volume
			wm8994_write(codec, 0x0020,  0x01F9);	//mixout L volume, not useful
			wm8994_write(codec, 0x0021,  0x01F9);	//mixout R volume, not useful
#endif
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			wm8994_write(codec, 0x0208,  0x001a);	//aif1dspclk ena, sysdspclk ena, sysclk src: aif1
			//HP_misc
			wm8994_write(codec, 0x0051,  0x0005);
			wm8994_write(codec, 0x0060,  0x0022);
			wm8994_write(codec, 0x004C,  0x9F25);
			wm8994_write(codec, 0x0055,  0x0541);
			wm8994_write(codec, 0x0055,  0x0401);
			wm8994_write(codec, 0x0054,  0x0303);
			wm8994_write(codec, 0x0057,  0xFBFB);
			wm8994_write(codec, 0x0054,  0x000F);
			wm8994_write(codec, 0x0060,  0x00EE);
			//power
			wm8994_write(codec, 0x0001,  0x3303);	//spkL,R ena, hpout1L,R ena
			wm8994_write(codec, 0x0002,  0x0000);	//input disable
			wm8994_write(codec, 0x0003,  0x0300);	//spkLvol, spkRvol ena
			/* wenpin.cui: Playback, DONOT operate ADC */
			//wm8994_write(codec, 0x0004,  0x0000);	//adc disable wenpin.cui
			wm8994_write(codec, 0x0005,  0x0303);	//aif1dacL, aif1dacR ena, dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0420,  0x0000);	//aif1dac un-mute
			break;
		}
		break;
	case PLAYBACK_SPK_INCALL:
		switch (1) {
		default:
			#if (1)
			//wm8994_write(codec,	0x0244,	 0x0002);   //FLL2 control, FLL2 source clk = LRCLK2
			wm8994_write(codec, 0x0204,  0x0019);	//AIF2CLK = FLL2, AIF2CLK enable
			wm8994_write(codec, 0x0211,  0x0009);	//AIF2 fs = 8khz, AIF2CLK/fs = 1536
			wm8994_write(codec, 0x0310,  0x4118);	//DSP mode A 
			wm8994_write(codec, 0x0208,  0x000F);	//SYSCLK = AIF2CLK
			wm8994_write(codec, 0x0312,  0x0000);	//AIF2 slave mode
			wm8994_write(codec, 0x0311,  0x0100);	//mono 
			#else
			wm8994_write(codec, 0x0204,  0x0001);	//AIF2CLK = MCLK1, AIF2CLK enable
			wm8994_write(codec, 0x0211,  0x0009);	//AIF2 fs = 8khz, AIF2CLK/fs = 1536
			wm8994_write(codec, 0x0310,  0x4118);	//DSP mode A 
			wm8994_write(codec, 0x0208,  0x000F);	//SYSCLK = AIF2CLK
			wm8994_write(codec, 0x0312,  0x0000);	//AIF2 slave mode
			wm8994_write(codec, 0x0311,  0x0100);	//mono 
			#endif
			//playback path
			wm8994_write(codec, 0x0601,  0x0004);	//aif2dacL --> dac1L
			wm8994_write(codec, 0x0602,  0x0004);	//aif2dacR --> dac1R
			wm8994_write(codec, 0x0036,  0x0003);	//dac1L --> spkmixL, dac1R --> spkmixR
			wm8994_write(codec, 0x0024,  0x0011);	//spkmixL --> spkoutL, spkmixR --> spkoutR
			//playback gain
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0612,  0x01C0);	//dac2L volume
			wm8994_write(codec, 0x0613,  0x01C0);	//dac2R volume
			wm8994_write(codec, 0x0502,  0x01C0);	//aif2 dac1 L volume
			wm8994_write(codec, 0x0503,  0x01C0);	//aif2 dac1 R volume
			wm8994_write(codec, 0x0022,  0x0000);	//spkmixL volume
			wm8994_write(codec, 0x0023,  0x0000);	//spkmixR volume
			wm8994_write(codec, 0x0026,  0x017C);	//spkoutL volume
			wm8994_write(codec, 0x0027,  0x017C);	//spkoutR volume
			wm8994_write(codec, 0x0025,  0x0164);	//spkoutL,R boost
			//capture path
			wm8994_write(codec, 0x0028,  0x0030);	//in1Lp,n --> in1L
			wm8994_write(codec, 0x0029,  0x0020);	//in1L -- > mixinL
			wm8994_write(codec, 0x0621,  0x0000);	//adcL,R sidetone mux
			wm8994_write(codec, 0x0604,  0x0010);	//adcL --> dac2L
			wm8994_write(codec, 0x0605,  0x0010);	//adcL --> dac2R		
			//capture gain
			wm8994_write(codec, 0x0018,  0x010B);	//in1L volume: +30dB
			wm8994_write(codec, 0x0029,  0x0030);	//in1L_mixinL vol: +30dB
			wm8994_write(codec, 0x0603,  0x000C);	//adcL_dac2_vol: 0db
			wm8994_write(codec, 0x0500,  0x01C0);	//aif2_adcL_vol: 0db
			wm8994_write(codec, 0x0501,  0x01C0);	//aif2_adcR_vol: 0db
			//mic
			wm8994_write(codec, 0x003A,  0x0002);	//micb1 level
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0208,  0x001F);	//aif2dspclk ena, sysdspclk ena, sysclk src: aif2
			//power
			wm8994_write(codec, 0x0001,  0x3013);	//spkL,R ena, micb1 ena
			wm8994_write(codec, 0x0002,  0x0240);	//mixinL ena, in1L ena
			wm8994_write(codec, 0x0003,  0x0300);	//spkLvol, spkRvol ena
			wm8994_write(codec, 0x0004,  0x3003);	//aif2adcL, aif2adcR ena, adcL, adcR ena
			wm8994_write(codec, 0x0005,  0x3003);	//aif2dacL, aif2dacR ena, dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0520,  0x0000);	//aif2dac un-mute
			break;
		}
		break;
	case PLAYBACK_HP_INCALL:
		switch (1) {
		default:
			
#if (1)
			//wm8994_write(codec,	0x0244,	 0x0002);   //FLL2 control, FLL2 source clk = LRCLK2
			wm8994_write(codec, 0x0204,  0x0019);	//AIF2CLK = FLL2, AIF2CLK enable
			wm8994_write(codec, 0x0211,  0x0009);	//AIF2 fs = 8khz, AIF2CLK/fs = 1536
			wm8994_write(codec, 0x0310,  0x4118);	//DSP mode A 
			wm8994_write(codec, 0x0208,  0x000F);	//SYSCLK = AIF2CLK
			wm8994_write(codec, 0x0312,  0x0000);	//AIF2 slave mode
			wm8994_write(codec, 0x0311,  0x0100);	//mono 
#else
			wm8994_write(codec, 0x0204,  0x0001);	//AIF2CLK = MCLK1, AIF2CLK enable, 409600
			wm8994_write(codec, 0x0211,  0x0009);	//AIF2 fs = 8khz, AIF2CLK / fs = 1536
			wm8994_write(codec, 0x0310,  0x4118);	//DSP mode A 
			wm8994_write(codec, 0x0208,  0x000F);	//SYSCLK = AIF2CLK
			wm8994_write(codec, 0x0312,  0x0000);	//AIF2 slave mode
			wm8994_write(codec, 0x0311,  0x0100);	//mono 
#endif
			//playback path
			wm8994_write(codec, 0x002D,  0x0100);	//dac1L --> hpout1Lvol
			wm8994_write(codec, 0x002E,  0x0100);	//dac1R --> hpout1Rvol
			wm8994_write(codec, 0x0601,  0x0004);	//aif2dacL --> dac1L
			wm8994_write(codec, 0x0602,  0x0004);	//aif2dacR --> dac1R
			wm8994_write(codec, 0x0036,  0x0000);	//dac1L !--> spkmixL, dac1R !--> spkmixR
			//playback gain
			wm8994_write(codec, 0x0610,  0x01C0);	//dac1L volume
			wm8994_write(codec, 0x0611,  0x01C0);	//dac1R volume
			wm8994_write(codec, 0x0612,  0x01C0);	//dac2L volume
			wm8994_write(codec, 0x0613,  0x01C0);	//dac2R volume
			wm8994_write(codec, 0x0502,  0x01C0);	//aif1 dac1 L volume
			wm8994_write(codec, 0x0503,  0x01C0);	//aif1 dac1 R volume
			wm8994_write(codec, 0x001C,  0x01ea);	//hpout1 L volume	//test
			wm8994_write(codec, 0x001D,  0x01ea);	//hpout1 R volume	//test
			wm8994_write(codec, 0x0020,  0x01F9);	//mixout L volume, not useful
			wm8994_write(codec, 0x0021,  0x01F9);	//mixout R volume, not useful
			wm8994_write(codec, 0x0025,  0x0164);	//spkoutL,R boost
			//capture path
			wm8994_write(codec, 0x0028,  0x0003);	//in1Rp,n --> in1R
			wm8994_write(codec, 0x002A,  0x0020);	//in1R -- > mixinR
			wm8994_write(codec, 0x0621,  0x0000);	//adcL,R sidetone mux
			wm8994_write(codec, 0x0604,  0x0020);	//adcR --> dac2L
			wm8994_write(codec, 0x0605,  0x0020);	//adcR --> dac2R
			//capture gain
			wm8994_write(codec, 0x001A,  0x010B);	//in1R volume: +30dB
			wm8994_write(codec, 0x002A,  0x0030);	//in1R_mixinR vol: +30dB
			wm8994_write(codec, 0x0603,  0x0180);	//adcR_dac2_vol: 0db
			wm8994_write(codec, 0x0500,  0x01C0);	//aif2_adcL_vol: 0db
			wm8994_write(codec, 0x0501,  0x01C0);	//aif2_adcR_vol: 0db
			//mic
			wm8994_write(codec, 0x003A,  0x0002);	//micb1 level
			wm8994_write(codec, 0x003B,  0x000D);	//LDO1		//test
			wm8994_write(codec, 0x003C,  0x0003);	//LDO2		//test
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0208,  0x001F);	//aif2dspclk ena, sysdspclk ena, sysclk src: aif2
			//HP_misc
			wm8994_write(codec, 0x0051,  0x0005);
			wm8994_write(codec, 0x0060,  0x0022);
			wm8994_write(codec, 0x004C,  0x9F25);
			wm8994_write(codec, 0x0055,  0x0541);
			wm8994_write(codec, 0x0055,  0x0401);
			wm8994_write(codec, 0x0054,  0x0303);
			wm8994_write(codec, 0x0057,  0xFBFB);
			wm8994_write(codec, 0x0054,  0x000F);
			wm8994_write(codec, 0x0060,  0x00EE);
			//power
			wm8994_write(codec, 0x0001,  0x0323);	//hpout1L,R ena, micb2 ena
			wm8994_write(codec, 0x0002,  0x0110);	//mixinR ena, in1R ena
			wm8994_write(codec, 0x0003,  0x0000);	//spkLvol, spkRvol !ena
			wm8994_write(codec, 0x0004,  0x3003);	//aif2adcL, aif2adcR ena, adcL, adcR ena
			wm8994_write(codec, 0x0005,  0x3003);	//aif2dacL, aif2dacR ena, dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0520,  0x0000);	//aif2dac un-mute
			break;
		}
		break;
	case PLAYBACK_BT_INCALL:
		switch (1) {
		default:
			wm8994_write(codec, 0x0204,  0x0001);	//AIF2CLK = MCLK1, AIF2CLK enable, 409600
			wm8994_write(codec, 0x0211,  0x0009);	//AIF2 fs = 8khz, AIF2CLK / fs = 1536
			wm8994_write(codec, 0x0310,  0x4118);	//DSP mode A 
			wm8994_write(codec, 0x0208,  0x000F);	//SYSCLK = AIF2CLK
			wm8994_write(codec, 0x0312,  0x0000);	//AIF2 slave mode
			wm8994_write(codec, 0x0311,  0x0100);	//mono 
			//path
			wm8994_write(codec, 0x0006,  0x0014);	//aif3_adcdat_src = dacdat2, aif2_adcdat_src = dacdat3
			//gain
			wm8994_write(codec, 0x0502,  0x01C0);	//aif2 dac1 L volume
			wm8994_write(codec, 0x0503,  0x01C0);	//aif2 dac1 R volume
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			//power
			wm8994_write(codec, 0x0001,  0x0003);
			wm8994_write(codec, 0x0002,  0x0000);
			wm8994_write(codec, 0x0003,  0x0000);	//spkLvol, spkRvol !ena
			wm8994_write(codec, 0x0004,  0x3000);	//aif2adcL, aif2adcR ena
			wm8994_write(codec, 0x0005,  0x3000);	//aif2dacL, aif2dacR ena
			//last
			wm8994_write(codec, 0x0520,  0x0000);	//aif2dac un-mute
			break;
		}
		break;
	case PLAYBACK_SPK_HP_INCALL:	
	case PLAYBACK_HS_INCALL:
	case PLAYBACK_SPK_HS_INCALL:		
	case PLAYBACK_REC_INCALL:		
		break;	
	case PLAYBACK_NONE:
		switch(capture_path) {
#ifdef WENPIN_DEBUG //wenpin.cui: debug only, all capture from boardmic
		case CAPTURE_MAIN_MIC_NORMAL:
		case CAPTURE_HAND_MIC_NORMAL:
			//path
			wm8994_write(codec, 0x0028,  0x0030);	//in1Lp,n --> in1L
			wm8994_write(codec, 0x0029,  0x0020);	//in1L -- > mixinL
			wm8994_write(codec, 0x0621,  0x0000);	//adcL,R sidetone mux
			wm8994_write(codec, 0x0604,  0x0000);	//adcL !--> dac2L
			wm8994_write(codec, 0x0605,  0x0000);	//adcL !--> dac2R
			wm8994_write(codec, 0x0606,  0x0002);	//adcL --> aif1adcL
			wm8994_write(codec, 0x0300,  0x2010);
			//gain
			wm8994_write(codec, 0x0018,  0x010B);	//in1L volume: +30dB
			wm8994_write(codec, 0x0029,  0x0030);	//in1L_mixinL vol: +30dB
			wm8994_write(codec, 0x0400,  0x01C0);	//aif1 adc1 L volume
			wm8994_write(codec, 0x0401,  0x01C0);	//aif1 adc1 R volume
			//mic
			wm8994_write(codec, 0x003A,  0x0000);	//micb1 level
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			wm8994_write(codec, 0x0208,  0x001a);	//aif1dspclk ena, sysdspclk ena, sysclk src: aif1
			//power
			wm8994_write(codec, 0x0001,  0x0013);	//!spkL,R ena, micb1 ena
			wm8994_write(codec, 0x0002,  0x0240);	//mixinL ena, in1L ena
			wm8994_write(codec, 0x0003,  0x0000);	//!spkLvol, spkRvol ena
			wm8994_write(codec, 0x0004,  0x0303);	//aif1adcL, aif1adcR ena, adcL, adcR ena
			/* Capture, DONOT operate DAC */
	//		wm8994_write(codec, 0x0005,  0x0000);	//!aif2dacL, aif2dacR ena, !dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0420,  0x0000);	//aif1dac un-mute
			break;
			
#else
		case CAPTURE_MAIN_MIC_NORMAL:
			//path
			wm8994_write(codec, 0x0028,  0x0030);	//in1Lp,n --> in1L
			wm8994_write(codec, 0x0029,  0x0020);	//in1L -- > mixinL
			wm8994_write(codec, 0x0621,  0x0000);	//adcL,R sidetone mux
			wm8994_write(codec, 0x0604,  0x0000);	//adcL !--> dac2L
			wm8994_write(codec, 0x0605,  0x0000);	//adcL !--> dac2R
			wm8994_write(codec, 0x0606,  0x0002);	//adcL --> aif1adcL
			wm8994_write(codec, 0x0300,  0x2010);
			//gain
			wm8994_write(codec, 0x0018,  0x010B);	//in1L volume: +30dB
			wm8994_write(codec, 0x0029,  0x0030);	//in1L_mixinL vol: +30dB
			wm8994_write(codec, 0x0400,  0x01C0);	//aif1 adc1 L volume
			wm8994_write(codec, 0x0401,  0x01C0);	//aif1 adc1 R volume
			//mic
			wm8994_write(codec, 0x003A,  0x0000);	//micb1 level
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			wm8994_write(codec, 0x0208,  0x001a);	//aif1dspclk ena, sysdspclk ena, sysclk src: aif1
			//power
			wm8994_write(codec, 0x0001,  0x0013);	//!spkL,R ena, micb1 ena
			wm8994_write(codec, 0x0002,  0x0240);	//mixinL ena, in1L ena
			wm8994_write(codec, 0x0003,  0x0000);	//!spkLvol, spkRvol ena
			wm8994_write(codec, 0x0004,  0x0303);	//aif1adcL, aif1adcR ena, adcL, adcR ena
			/* Capture, DONOT operate DAC */
			//wm8994_write(codec, 0x0005,  0x0000);	//!aif2dacL, aif2dacR ena, !dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0420,  0x0000);	//aif1dac un-mute
			break;
		case CAPTURE_HAND_MIC_NORMAL:
			//path
			wm8994_write(codec, 0x0028,  0x0003);	//in1Rp,n --> in1R
			wm8994_write(codec, 0x002A,  0x0020);	//in1R -- > mixinR
			wm8994_write(codec, 0x0621,  0x0000);	//adcL,R sidetone mux
			wm8994_write(codec, 0x0604,  0x0000);	//adcR !--> dac2L
			wm8994_write(codec, 0x0605,  0x0000);	//adcR !--> dac2R
			wm8994_write(codec, 0x0607,  0x0002);	//adcR --> aif1adcR
			wm8994_write(codec, 0x0300,  0xe010);
			//gain
			wm8994_write(codec, 0x001A,  0x010B);	//in1R volume: +30dB
			wm8994_write(codec, 0x002A,  0x0030);	//in1R_mixinR vol: +30dB
			wm8994_write(codec, 0x0400,  0x01C0);	//aif1_adcL_vol: 0db
			wm8994_write(codec, 0x0401,  0x01C0);	//aif1_adcR_vol: 0db
			//mic
			wm8994_write(codec, 0x003A,  0x0000);	//micb1 level
			//clock
			wm8994_write(codec, 0x0200,  0x0001);	//aif1clk ena
			wm8994_write(codec, 0x0204,  0x0001);	//aif2clk ena
			wm8994_write(codec, 0x0208,  0x001a);	//aif1dspclk ena, sysdspclk ena, sysclk src: aif1
			//power
			wm8994_write(codec, 0x0001,  0x0023);	//!spkL,R ena, micb2 ena
			wm8994_write(codec, 0x0002,  0x6110);	//mixinR ena, in1R ena
			wm8994_write(codec, 0x0003,  0x0000);	//!spkLvol, spkRvol ena
			wm8994_write(codec, 0x0004,  0x0101);	//!aif1adcL, aif1adcR ena, adcL, adcR ena
			/* Capture, DONOT operate DAC */
			//wm8994_write(codec, 0x0005,  0x0000);	//!aif2dacL, aif2dacR ena, !dacL, dacR ena
			wm8994_write(codec, 0x0006,  0x0000);	//default
			//last
			wm8994_write(codec, 0x0420,  0x0000);	//aif1dac un-mute
			break;
#endif
		case CAPTURE_NONE:
			printk("Mute playback\n");
			wm8994_write(codec, 0x0022,  0x0003);	//spkmixL volume Mute
			wm8994_write(codec, 0x0023,  0x0003);	//spkmixR volume Mute
			wm8994_write(codec, 0x001C,  0x01BF);	//hpout1 L volume Mute
			wm8994_write(codec, 0x001D,  0x01BF);	//hpout1 R volume, maximum
		default:
			wm8994_write(codec, 0x0001,  0x0003);	//output off
			break;
		}
		break;
	default:			
		printk("%s(): Invalid path  %d\n", __func__,playback_path);
		ret = -1;
		break;			
	}

#ifdef DEBUG
	wm8994_dump(codec);
#endif
	return ret;	
}

int set_capture_path(struct snd_soc_codec *codec,u8 playback_path,
		u8 capture_path, enum snd_soc_bias_level level)
{
	int ret = 0;

	dprintk ("%s() :playbackpath = %d,capture_path %d\n", __func__,playback_path,capture_path);
	switch(capture_path) {
		case CAPTURE_MAIN_MIC_NORMAL:
			switch(playback_path) {
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:			
			case PLAYBACK_NONE:
			default:
				break;			
			}
			break;			
		case CAPTURE_HAND_MIC_NORMAL:
			switch(playback_path) {
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:			
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:		
			case PLAYBACK_NONE:
			default:
				break;			
			}
			break;			
		case CAPTURE_MAIN_MIC_INCALL:
			switch(playback_path) {
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:			
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:		
			default:
				break;			
			}
			break;

		case CAPTURE_HAND_MIC_INCALL:	
			switch(playback_path) {
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING:			
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:		
			default:
				break;			
			}
			break;
		case CAPTURE_NONE:
			switch(playback_path) {
			case PLAYBACK_REC_NORMAL:
			case PLAYBACK_SPK_NORMAL:
			case PLAYBACK_REC_RING:
			case PLAYBACK_SPK_RING: 
			case PLAYBACK_HP_NORMAL:					
			case PLAYBACK_SPK_HP_NORMAL:
			case PLAYBACK_HP_RING:
			case PLAYBACK_SPK_HP_RING:
			default:
				break;
			}
			break;
		default:			
			break;			
	}
	return ret;	
}
