/*
 *  smdk_wm8994.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <mach/regs-clock.h>

#include "i2s.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8994.h"

#if 0
#define printa(x...) 	printk(x)
#else
#define printa(x...)
#endif
 /*
  * Default CFG switch settings to use this driver:
  *	SMDKV310: CFG5-1000, CFG7-111111
  */

 /*
  * Configure audio route as :-
  * $ amixer sset 'DAC1' on,on
  * $ amixer sset 'Right Headphone Mux' 'DAC'
  * $ amixer sset 'Left Headphone Mux' 'DAC'
  * $ amixer sset 'DAC1R Mixer AIF1.1' on
  * $ amixer sset 'DAC1L Mixer AIF1.1' on
  * $ amixer sset 'IN2L' on
  * $ amixer sset 'IN2L PGA IN2LN' on
  * $ amixer sset 'MIXINL IN2L' on
  * $ amixer sset 'AIF1ADC1L Mixer ADC/DMIC' on
  * $ amixer sset 'IN2R' on
  * $ amixer sset 'IN2R PGA IN2RN' on
  * $ amixer sset 'MIXINR IN2R' on
  * $ amixer sset 'AIF1ADC1R Mixer ADC/DMIC' on
  */

#ifdef TC4
#define SMDK_WM8994_FREQ    (24000000)  // 24MHz(XUSBXTI XCLKOUT)
#else
/* SMDK has a 16.934MHZ crystal attached to WM8994 */
#define SMDK_WM8994_FREQ 16934000
#endif

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

//	printk("[zsb] set_epll_rate : %ld\n", rate);		// 45158400
	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}
#endif /* CONFIG_SND_SAMSUNG_I2S_MASTER */

#ifndef CONFIG_SND_SAMSUNG_I2S_MASTER
static int smdk_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	printa("[zsb] smdk_hw_params : codec master\n");
	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
					SMDK_WM8994_FREQ, pll_out);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	return 0;
}
#else /* CONFIG_SND_SAMSUNG_I2S_MASTER */
static int smdk_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk;

	printa("[zsb] smdk_hw_params : codec slave\n");
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	printa("[zsb] smdk_hw_params rate : %d\n", params_rate(params));
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);
	printa("[zsb] rclk = %ld, psr = %d, bfs = %d\n", rclk, psr, bfs);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK1,
					rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	printa("[zsb] pos6\n");
	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_PRESCALER, psr);		// zsb add important
	if (ret < 0)
		return ret;
	
	
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2, WM8994_FLL_SRC_BCLK,     //open FLL2 for voice call when AP sleep. xujie_2012.03.09
					139264, 12582912);                                        //source clk: 8K*17, output clk: 8k*1536
	if (ret < 0)
		return ret;
	
	return 0;
}
#endif /* CONFIG_SND_SAMSUNG_I2S_MASTER */

/*
 * SMDK WM8994 DAI operations.
 */
static struct snd_soc_ops smdk_ops = {
	.hw_params = smdk_hw_params,
};

static int smdk_wm8994_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

#ifdef TC4
	snd_soc_dapm_enable_pin(dapm, "HP");
	snd_soc_dapm_enable_pin(dapm, "MAIN MIC");
	snd_soc_dapm_enable_pin(dapm, "SUB MIC");
	snd_soc_dapm_enable_pin(dapm, "SPK");

	/* Other pins NC */
	snd_soc_dapm_nc_pin(dapm, "IN1LN");
	snd_soc_dapm_nc_pin(dapm, "IN1LP");
	snd_soc_dapm_nc_pin(dapm, "IN1RN");
	snd_soc_dapm_nc_pin(dapm, "IN1RP");
	snd_soc_dapm_nc_pin(dapm, "HPOUT1N");
	snd_soc_dapm_nc_pin(dapm, "HPOUT1P");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTLN");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTLP");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRN");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRP");
	snd_soc_dapm_nc_pin(dapm, "HPOUT2P");
	snd_soc_dapm_nc_pin(dapm, "HPOUT2N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1P");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2P");
	snd_soc_dapm_nc_pin(dapm, "IN2LN");
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN2RN");
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");
#else
	/* HeadPhone */
	snd_soc_dapm_enable_pin(dapm, "HPOUT1R");
	snd_soc_dapm_enable_pin(dapm, "HPOUT1L");

	/* MicIn */
	snd_soc_dapm_enable_pin(dapm, "IN1LN");
	snd_soc_dapm_enable_pin(dapm, "IN1RN");

	/* LineIn */
	snd_soc_dapm_enable_pin(dapm, "IN2LN");
	snd_soc_dapm_enable_pin(dapm, "IN2RN");

	/* Other pins NC */
	snd_soc_dapm_nc_pin(dapm, "HPOUT2P");
	snd_soc_dapm_nc_pin(dapm, "HPOUT2N");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTLN");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTLP");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRP");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRN");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1P");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2P");
	snd_soc_dapm_nc_pin(dapm, "IN1LP");
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN1RP");
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");
#endif
	snd_soc_dapm_sync(dapm);

	return 0;
}

#ifdef TC4
#ifndef CONFIG_SND_SAMSUNG_I2S_MASTER

/*
 *  Function to set the MCLK of WM8994 as XCLKOUT(XSUBXTI, 24MHz)
 */
static void set_xusbxti_for_xclkout(void)
{
	u32 val = 0;

	val = readl(S5P_PMU_DEBUG);
	val &= ~(S5P_PMU_CLKOUT_SEL_MASK);
	val |= (S5P_PMU_CLKOUT_SEL_XUSBXTI<<S5P_PMU_CLKOUT_SEL_SHIFT);
	writel(val, S5P_PMU_DEBUG);

    return;
}
#endif
#endif

static struct snd_soc_dai_link smdk_dai[] = {
	{ /* Primary DAI i/f */
		.name = "WM8994 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.init = smdk_wm8994_init_paiftx,
		.ops = &smdk_ops,
	}, { /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "wm8994-aif1",
#if defined(CONFIG_SND_SAMSUNG_NORMAL) || defined(CONFIG_SND_SAMSUNG_RP)
		.platform_name = "samsung-audio",
#else
		.platform_name = "samsung-audio-idma",
#endif
		.codec_name = "wm8994-codec",
		.init = smdk_wm8994_init_paiftx,
		.ops = &smdk_ops,
	},

};

static struct snd_soc_card smdk = {
	.name = "SMDK-I2S",
	.dai_link = smdk_dai,

	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(smdk_dai). */
	.num_links = ARRAY_SIZE(smdk_dai),
};

static struct platform_device *smdk_snd_device;

static int __init smdk_audio_init(void)
{
	int ret;
#ifdef TC4
#ifndef CONFIG_SND_SAMSUNG_I2S_MASTER
    set_xusbxti_for_xclkout();
#endif
#endif

	smdk_snd_device = platform_device_alloc("soc-audio", -1);
	if (!smdk_snd_device)
		return -ENOMEM;

	platform_set_drvdata(smdk_snd_device, &smdk);

	ret = platform_device_add(smdk_snd_device);
	if (ret)
		platform_device_put(smdk_snd_device);

	return ret;
}
module_init(smdk_audio_init);

static void __exit smdk_audio_exit(void)
{
	platform_device_unregister(smdk_snd_device);
}
module_exit(smdk_audio_exit);

MODULE_DESCRIPTION("ALSA SoC SMDK WM8994");
MODULE_LICENSE("GPL");
