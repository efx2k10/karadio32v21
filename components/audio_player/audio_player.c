/*
 * audio_player.c
 *
 *  Created on: 12.03.2017
 *      Author: michaelboeckling
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "audio_player.h"
#include "spiram_fifo.h"
#include "freertos/task.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_system.h"
#include "esp_log.h"

#include "fdk_aac_decoder.h"
//#include "helix_aac_decoder.h"
//#include "libfaad_decoder.h"
#include "mp3_decoder.h"
#include "webclient.h"
#include "vs1053.h"
#include "app_main.h"

#define TAG "audio_player"
//#define PRIO_MAD configMAX_PRIORITIES - 4


static player_t *player_instance = NULL;
static component_status_t player_status = UNINITIALIZED;

static int start_decoder_task(player_t *player)
{
    TaskFunction_t task_func;
    char * task_name;
    uint16_t stack_depth;
	int priority = PRIO_MAD;

    ESP_LOGD(TAG, "RAM left %d", esp_get_free_heap_size());
	if (get_audio_output_mode() == VS1053)
	{
		task_func = vsTask;
        task_name = (char*)"vsTask";
        stack_depth = 3000;
		priority = PRIO_VS1053;
	} else
    switch (player->media_stream->content_type)
    {
        case AUDIO_MPEG:
            task_func = mp3_decoder_task;
            task_name = (char*)"mp3_decoder_task";
            stack_depth = 8448;
            break;

 /*       case AUDIO_MP4:
            task_func = libfaac_decoder_task;
            task_name = (char*)"libfaac_decoder_task";
            stack_depth = 55000;
            break;
*/

		case AUDIO_AAC:
        case OCTET_STREAM: // probably .aac
			if (!bigSram())
			{
				ESP_LOGE(TAG, "aac mime not supported on WROOM cpu, type: %d", player->media_stream->content_type);
				spiRamFifoReset();
				clientDisconnect("AAC Not played");
				return -1;				
			}
		
            task_func = fdkaac_decoder_task;
            task_name = (char*)"fdkaac_decoder_task";
            stack_depth = 6900; //6144; 
            break;
			
/*
		case AUDIO_AAC:
        case OCTET_STREAM: // probably .aac
            task_func = helixaac_decoder_task;
            task_name = (char*)"helixaac_decoder_task";
            stack_depth = 6144; //6144; //6144
            break;
*/
/*				
        case AUDIO_AAC:
        case OCTET_STREAM: // probably .aac
            task_func = libfaac_decoder_task;
            task_name = "libfaac_decoder_task";
            stack_depth = 55000;
            break;
*/
        default:
            ESP_LOGW(TAG, "unknown mime type: %d", player->media_stream->content_type);
			spiRamFifoReset();
            return -1;
    }

	if (((task_func != NULL)) && (xTaskCreatePinnedToCore(task_func, task_name, stack_depth, player,
			priority, NULL, CPU_MAD) != pdPASS)) 
	{
									
		ESP_LOGE(TAG, "ERROR creating decoder task! Out of memory?");
		spiRamFifoReset();
		return -1;
	} else {
		player->decoder_status = RUNNING;
	}
	
	ESP_LOGD(TAG, "decoder task created: %s", task_name);

    return 0;
}

static int t;

/* Writes bytes into the FIFO queue, starts decoder task if necessary. */
int audio_stream_consumer(const char *recv_buf, ssize_t bytes_read,
        void *user_data)
{
//	ESP_LOGW(TAG,"audio_stream_consumer rec len: %d",bytes_read);
    player_t *player = user_data;
    // don't bother consuming bytes if stopped
    if(player->command == CMD_STOP) {
		clientSilentDisconnect();
        return -2;
    }

    if (bytes_read > 0) {
        spiRamFifoWrite(recv_buf, bytes_read);
    }


	int bytes_in_buf = spiRamFifoFill();
	uint8_t fill_level = (bytes_in_buf * 100) / spiRamFifoLen();

	// seems 4k is enough to prevent initial buffer underflow
//	uint8_t min_fill_lvl = player->buffer_pref == BUF_PREF_FAST ? 40 : 90;
//	bool buffer_ok = fill_level > min_fill_lvl;

	if (player->decoder_status != RUNNING ) 
	{
		t = 0;
//		uint32_t trigger = (bigSram())? (70*1024):(20*1024);
//		bool buffer_ok = (bytes_in_buf > trigger);
		bool buffer_ok = (fill_level > (bigSram()?10:70)); // in %
		if (buffer_ok)
		{
		// buffer is filled, start decoder
//			ESP_LOGV(TAG,"trigger: %d",trigger);
			if (start_decoder_task(player) != 0) {
				ESP_LOGE(TAG, "failed to start decoder task");
				audio_player_stop();
				clientDisconnect("decoder failed"); 
				return -1;
			}
			ESP_LOGI(TAG, "Buffer fill %u%%, %d // %d bytes", fill_level, bytes_in_buf,spiRamFifoLen());
		}
	}

	t = (t+1) & 255;
	if (t == 0) {
		ESP_LOGI(TAG, "Buffer fill %u%%, %d // %d bytes", fill_level, bytes_in_buf,spiRamFifoLen());
	}

    return 0;
}

void audio_player_init(player_t *player)
{
    player_instance = player;
    player_status = INITIALIZED;
}

void audio_player_destroy()
{
    if (get_audio_output_mode() != VS1053) renderer_destroy();
    player_status = UNINITIALIZED;
}

void audio_player_start()
{
		if (get_audio_output_mode() != VS1053) renderer_start();
		player_instance->media_stream->eof = false;
		player_instance->command = CMD_START;
		player_instance->decoder_command = CMD_NONE;	
		player_status = RUNNING;
}

void audio_player_stop()
{ 
		player_instance->decoder_command = CMD_STOP;
		player_instance->command = CMD_STOP;
		player_instance->media_stream->eof = true;
		if (get_audio_output_mode() != VS1053)renderer_stop();
		player_instance->command = CMD_NONE;
		player_status = STOPPED;
}

component_status_t get_player_status()
{
    return player_status;
}

