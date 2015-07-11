/*
 * smdk4x12_wm8960.c  --  WM8960 ALSA SoC Audio driver
 *
 * Author: cym
 *
 *www.topeet.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/clk.h>
#include <linux/io.h>

#include "i2s.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8960.h"

//#define DEBUG	1

#ifdef	DEBUG
#define	dprintk( argc, argv... )		printk( argc, ##argv )
#else
#define	dprintk( argc, argv... )		
#endif

static struct snd_soc_card smdk4x12_soc_card;

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

        clk_set_rate(fout_epll, rate);
		
out:
        clk_put(fout_epll);

        return 0;
}

static int smdk4x12_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rate = params_rate(params);
	snd_pcm_format_t fmt = params_format( params );

	int bclk_div = 0, bfs, psr = 0, rfs = 0, rclk_div = 0, ret = 0;

	unsigned int mul = 0;

	unsigned long rclk = 0;

	dprintk("+%s()\n", __FUNCTION__);

	switch (fmt){
		case SNDRV_PCM_FORMAT_S16:
			bclk_div = 32;
			break;
			
		case SNDRV_PCM_FORMAT_S20_3LE:
		case SNDRV_PCM_FORMAT_S24:
			bclk_div = 48;
			break;
			
		default:
			dprintk("-%s(): PCM FMT ERROR\n", __FUNCTION__);
			return -EINVAL;
	}

	switch (params_format(params)){
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

	switch (rate){
		case 8000:
		case 11025:
		case 12000:
			if(bclk_div == 48)
				rclk_div = 768;	//0x300
			else
				rclk_div = 512;	//0x200
			break;
			
		case 16000:
		case 22050:
		case 24000:
		case 32000:
		case 44100:	//AC44
		case 48000:
		case 88200:
		case 96000:
			if(bclk_div == 48)
				rclk_div = 384;	//0x180
			else
				rclk_div = 256;	//0x100
			break;
		
		case 64000:
			rclk_div = 384;		//0x180
			break;
		
		default:
			dprintk("-%s(): SND RATE ERROR\n", __FUNCTION__);
			return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk){
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
	dprintk("[zsb] rclk = %ld, psr = %d, bfs = %d\n", rclk, psr, bfs);
	
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
										| SND_SOC_DAIFMT_NB_NF
										| SND_SOC_DAIFMT_CBS_CFS);
	if(ret < 0)
	{
		dprintk("-%s(): Codec DAI configuration error, %d\n", __FUNCTION__, ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
										| SND_SOC_DAIFMT_NB_NF
										| SND_SOC_DAIFMT_CBS_CFS);
	if( ret < 0 )
	{
		dprintk("-%s(): AP DAI configuration error, %d\n", __FUNCTION__, ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK, 0, SND_SOC_CLOCK_OUT);
	if(ret < 0)
	{
		dprintk("-%s(): AP sycclk CDCLK setting error, %d\n", __FUNCTION__, ret);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKDIV, WM8960_SYSCLK_DIV_1);
	if( ret < 0 )
	{
		dprintk("-%s(): Codec SYSCLKDIV setting error, %d\n", __FUNCTION__, ret);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_DACDIV, WM8960_DAC_DIV_1);
	if(ret < 0)
	{
		dprintk("-%s(): Codec DACDIV setting error, %d\n", __FUNCTION__, ret);
		return ret;
	}

	mul = rate * rclk_div;
	
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_DCLKDIV, mul);
	if( ret < 0 )
	{
		dprintk( "-%s(): Codec DCLKDIV setting error, %d\n", __FUNCTION__, ret );
		return ret;
	}
	
	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if(ret < 0)
	{
		dprintk("-%s(): AP prescalar setting error, %d\n", __FUNCTION__, ret);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_PRESCALER, psr);
	if(ret < 0)
	{
		dprintk("-%s(): AP RFS setting error, %d\n", __FUNCTION__, ret);
		return ret;
	}
	
	dprintk("-%s()\n", __FUNCTION__);
	
	return 0;
}

static struct snd_soc_ops smdk4x12_wm8960_ops = {
	.hw_params = smdk4x12_hw_params,
};

static const struct snd_soc_dapm_widget smdk4x12_dapm_capture_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line Input 3 (FM)", NULL),
};

static const struct snd_soc_dapm_widget smdk4x12_dapm_playback_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker_L", NULL),
	SND_SOC_DAPM_SPK("Speaker_R", NULL),
};

static const struct snd_soc_dapm_route smdk4x12_audio_map[] = {
	{"Headphone Jack", NULL, "HP_L"},
	{"Headphone Jack", NULL, "HP_R"},
	{"Speaker_L", NULL, "SPK_LP"},
	{"Speaker_L", NULL, "SPK_LN"},
	{"Speaker_R", NULL, "SPK_RP"},
	{"Speaker_R", NULL, "SPK_RN"},
	{"LINPUT1", NULL, "MICB"},
	{"MICB", NULL, "Mic Jack"},
};

static int smdk4x12_wm8960_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	dprintk("+%s()\n", __FUNCTION__);

	snd_soc_dapm_nc_pin(dapm, "RINPUT1");
	snd_soc_dapm_nc_pin(dapm, "LINPUT2");
	snd_soc_dapm_nc_pin(dapm, "RINPUT2");
	snd_soc_dapm_nc_pin(dapm, "OUT3");
	
	snd_soc_dapm_new_controls(dapm, smdk4x12_dapm_capture_widgets, ARRAY_SIZE(smdk4x12_dapm_capture_widgets));
	snd_soc_dapm_new_controls(dapm, smdk4x12_dapm_playback_widgets, ARRAY_SIZE(smdk4x12_dapm_playback_widgets));

	snd_soc_dapm_add_routes(dapm, smdk4x12_audio_map, ARRAY_SIZE(smdk4x12_audio_map));

	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_enable_pin(dapm, "Mic Jack");
	snd_soc_dapm_enable_pin(dapm, "Speaker_L");
	snd_soc_dapm_enable_pin(dapm, "Speaker_R");
	
	snd_soc_dapm_disable_pin(dapm, "Line Input 3 (FM)");

	dprintk("*%s(): dapm sync start\n", __FUNCTION__);
	snd_soc_dapm_sync( dapm );
	dprintk("*%s(): dapm sync end\n", __FUNCTION__);

	dprintk("-%s()\n", __FUNCTION__);
	
	return 0;
}

static struct snd_soc_dai_link smdk4x12_dai = {
	.name = "WM8960 AIF1",
	.stream_name = "Pri_Dai",
	.codec_name = "wm8960-codec.4-001a",
	.platform_name = "samsung-audio",
	.cpu_dai_name = "samsung-i2s.0",
	.codec_dai_name = "wm8960-hifi",
	.init = smdk4x12_wm8960_init,
	.ops = &smdk4x12_wm8960_ops,
};

static struct snd_soc_card smdk4x12_soc_card = {
	.name = "TOPEET-WM8960",
	.dai_link = &smdk4x12_dai,
	.num_links = 1,
};

static struct platform_device *smdk4x12_snd_device;

static int __init smdk4x12_audio_init(void)
{
	int ret;
	
	dprintk("+%s()\n", __FUNCTION__);

	smdk4x12_snd_device = platform_device_alloc("soc-audio", -1);
	if(!smdk4x12_snd_device)
	{
		dprintk("-%s() : platform_device_alloc failed\n", __FUNCTION__);
		return -ENOMEM;
	}

	platform_set_drvdata(smdk4x12_snd_device, &smdk4x12_soc_card);

	ret = platform_device_add(smdk4x12_snd_device);
	if(ret)
	{
		platform_device_put(smdk4x12_snd_device);
	}

	dprintk("-%s()\n", __FUNCTION__);
	
	return ret;
}

static void __exit smdk4x12_audio_exit(void)
{
	dprintk("+%s()\n", __FUNCTION__);
	
	platform_device_unregister(smdk4x12_snd_device);
	
	dprintk("-%s()\n", __FUNCTION__);
}


module_init(smdk4x12_audio_init);
module_exit(smdk4x12_audio_exit);

MODULE_AUTHOR("caoym@topeet.com");
MODULE_DESCRIPTION("ALSA SoC Exynos 4412+WM8960, ***** www.topeet.com*****");
MODULE_LICENSE("GPL");




