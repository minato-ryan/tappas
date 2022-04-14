#!/bin/bash
set -e

function init_variables() {
    print_help_if_needed $@
    script_dir=$(dirname $(realpath "$0"))
    source $script_dir/../../../../scripts/misc/checks_before_run.sh

    readonly POSTPROCESS_DIR="$TAPPAS_WORKSPACE/apps/gstreamer/x86/libs/"
    readonly RESOURCES_DIR="$TAPPAS_WORKSPACE/apps/gstreamer/x86/facial_landmarks/resources"
    readonly DEFAULT_POSTPROCESS_SO="$POSTPROCESS_DIR/libfacial_landmarks_post.so"
    readonly DEFAULT_DRAW_SO="$POSTPROCESS_DIR/libfacial_landmarks_draw.so"
    readonly DEFAULT_VIDEO_SOURCE="$RESOURCES_DIR/faces_120_120.mp4"
    readonly DEFAULT_HEF_PATH="$RESOURCES_DIR/tddfa_mobilenet_v1.hef"

    hef_path=$DEFAULT_HEF_PATH
    input_source=$DEFAULT_VIDEO_SOURCE
    postprocess_so=$DEFAULT_POSTPROCESS_SO
    draw_so=$DEFAULT_DRAW_SO

    print_gst_launch_only=false
    additonal_parameters=""

    video_sink_element=$([ "$XV_SUPPORTED" = "true" ] && echo "xvimagesink" || echo "ximagesink")
    hailo_bus_id=$(hailortcli scan | awk '{ print $NF }' | tail -n 1)
}

function print_usage() {
    echo "Face Landmarks pipeline usage:"
    echo ""
    echo "Options:"
    echo "  --help                  Show this help"
    echo "  -i INPUT --input INPUT  Set the video source (only videos allowed) (default $input_source)"
    echo "  --show-fps              Printing fps"
    echo "  --print-gst-launch      Print the ready gst-launch command without running it"
    exit 0
}

function print_help_if_needed() {
    while test $# -gt 0; do
        if [ "$1" = "--help" ] || [ "$1" == "-h" ]; then
            print_usage
        fi

        shift
    done
}

function parse_args() {
    while test $# -gt 0; do
        if [ "$1" = "--show-fps" ]; then
            echo "Printing fps"
            additonal_parameters="-v 2>&1 | grep hailo_display"
        elif [ "$1" = "--print-gst-launch" ]; then
            print_gst_launch_only=true
        elif [ "$1" = "--input" ] || [ "$1" = "-i" ]; then
            input_source="$2"
            shift
        else
            echo "Received invalid argument: $1. See expected arguments below:"
            print_usage
            exit 1
        fi
        shift
    done
}

init_variables $@
parse_args $@

# If the video provided is from a camera
if [[ $input_source =~ "/dev/video" ]]; then
    echo "Received invalid video source: $input_source - Only videos are allowed."
    exit 1
fi

PIPELINE="gst-launch-1.0 \
    filesrc location=$input_source name=src_0 ! decodebin ! \
    videoscale ! video/x-raw, pixel-aspect-ratio=1/1 ! videoconvert ! \
    queue leaky=no max_size_buffers=30 max-size-bytes=0 max-size-time=0 ! \
    hailonet hef-path=$hef_path device-id=$hailo_bus_id debug=False is-active=true qos=false ! \
    queue leaky=no max_size_buffers=30 max-size-bytes=0 max-size-time=0 ! \
    hailofilter2 so-path=$postprocess_so qos=false ! \
    queue leaky=no max_size_buffers=30 max-size-bytes=0 max-size-time=0 ! \
    hailooverlay ! videoconvert ! \
    fpsdisplaysink video-sink=$video_sink_element name=hailo_display sync=true text-overlay=false ${additonal_parameters}"

echo ${PIPELINE}
if [ "$print_gst_launch_only" = true ]; then
    exit 0
fi
eval ${PIPELINE}
