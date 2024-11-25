#pragma once

#include "switchboard.hpp"

#include <opencv2/core/mat.hpp>

namespace ILLIXR {

struct cam_type : switchboard::event {
    time_point time;
    cv::Mat    img0;
    cv::Mat    img1;

    cam_type(time_point _time, cv::Mat _img0, cv::Mat _img1)
        : time{_time}
        , img0{std::move(_img0)}
        , img1{std::move(_img1)} { }
};

struct eye_type : switchboard::event {
    time_point time;
    cv::Mat    img0;
    cv::Mat    img1;

    eye_type(time_point _time, cv::Mat _img0, cv::Mat _img1)
        : time{_time}
        , img0{std::move(_img0)}
        , img1{std::move(_img1)} { }
};

struct gaze_type : switchboard::event {
    time_point time;
    std::vector<double> gaze0;
    std::vector<double> gaze1;

    gaze_type(time_point _time, std::vector<double>& _gaze0, std::vector<double>& _gaze1)
        : time{_time}
        , gaze0{std::move(_gaze0)}
        , gaze1{std::move(_gaze1)} { }
};

struct host_image_type : switchboard::event {
    int width, height;
    std::vector<uint8_t> host_image;

    host_image_type(std::vector<uint8_t>& _host_image,
        int _width, int _height)
        : host_image{std::move(_host_image)}
        , width{_width}
        , height{_height} { }
};

struct rgb_depth_type : public switchboard::event {
    [[maybe_unused]] time_point time;
    cv::Mat                     rgb;
    cv::Mat                     depth;

    rgb_depth_type(time_point _time, cv::Mat _rgb, cv::Mat _depth)
        : time{_time}
        , rgb{std::move(_rgb)}
        , depth{std::move(_depth)} { }
};

} // namespace ILLIXR