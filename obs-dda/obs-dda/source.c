#include <Windows.h>
#include <obs.h>
#include <util/threading.h>
#include <util/platform.h>
#include "source.h"
#include "log.h"
#include "capture.h"

struct obs_source_info dda_source =
{
    .id = "dda",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO,
    .get_name = dda_getname,
    .create = dda_create,
    .destroy = dda_destroy,
    .get_properties = dda_properties,
};

struct dda_thread_info
{
    obs_source_t* source;
    os_event_t* stop_signal;
    pthread_t thread;
    bool initialized;
};

static void* video_thread(void* data)
{
    struct dda_thread_info* thread_info = data;
    uint32_t pixels[20 * 20];
    uint64_t cur_time = os_gettime_ns();

    struct obs_source_frame frame = 
    {
        .data = { [0] = (uint8_t*)pixels},
        .linesize = { [0] = 20 * 4},
        .width = 20,
        .height = 20,
        .format = VIDEO_FORMAT_BGRX,
    };

    while (os_event_try(thread_info->stop_signal) == EAGAIN)
    {
        capture_bitmap* output = capture_get_frame();
        frame.width = output->width;
        frame.height = output->height;
        frame.data[0] = output->buffer;
        frame.linesize[0] = frame.width * 4;

        frame.timestamp = cur_time;

        obs_source_output_video(thread_info->source, &frame);
    }

    return NULL;
}

static const char* dda_getname(void* unused)
{
    UNUSED_PARAMETER(unused);
    return "DDA Capture";
}

static void* dda_create(obs_data_t* settings, obs_source_t* source)
{
    struct dda_thread_info* thread_info = bzalloc(sizeof(struct dda_thread_info));
    thread_info->source = source;

    if (os_event_init(&thread_info->stop_signal, OS_EVENT_TYPE_MANUAL) != 0)
    {
        dda_destroy(thread_info);
        error("Failed to initialize event!");
        return NULL;
    }

    if (pthread_create(&thread_info->thread, NULL, video_thread, thread_info) != 0)
    {
        dda_destroy(thread_info);
        error("Failed to create thread!");
        return NULL;
    }

    if (!capture_init())
    {
        dda_destroy(thread_info);
        error("Failed to init DDA!");
        return NULL;
    }

    thread_info->initialized = true;

    UNUSED_PARAMETER(settings);
    UNUSED_PARAMETER(source);
    return thread_info;
}

static void dda_destroy(void* data)
{
    struct dda_thread_info* thread_info = data;

    if (thread_info)
    {
        if (thread_info->initialized)
        {
            os_event_signal(thread_info->stop_signal);
            pthread_join(thread_info->thread, NULL);
        }

        os_event_destroy(thread_info->stop_signal);
        bfree(thread_info);
    }
}

static bool display_settings_changed(obs_properties_t* props, obs_property_t* p, obs_data_t* settings)
{
    //obs_properties_get(props, "display_number");
    int display = obs_data_get_int(settings, "display_number");
    capture_change_display(display);
}

static obs_properties_t* dda_properties(void* data)
{
    obs_properties_t* props = obs_properties_create();

    obs_property_t* p = obs_properties_add_int(props, "display_number", "Display to capture", 0, 10, 1);

    obs_property_set_modified_callback(p, display_settings_changed);

    return props;
}