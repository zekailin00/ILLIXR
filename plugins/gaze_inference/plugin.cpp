#include "illixr/opencv_data_types.hpp"
#include "illixr/data_format.hpp"
#include "illixr/phonebook.hpp"
#include "illixr/relative_clock.hpp"
#include "illixr/threadloop.hpp"

#include <chrono>
#include <shared_mutex>
#include <thread>

#include "dirent.h"
#include "image_classifier.h"


using namespace ILLIXR;


class gaze_inference : public threadloop {
public:
    gaze_inference(const std::string& name_, phonebook* pb_)
        : threadloop{name_, pb_}
        , sb{pb->lookup_impl<switchboard>()}
        , _m_eye_subscriber{sb->get_reader<eye_type>("eye")}
        , _m_gaze_publisher{sb->get_writer<gaze_type>("gaze")}
    {
        spdlogger(std::getenv("gaze_inference_LOG_LEVEL"));
    }

    skip_option _p_should_skip() override {
        if (true) {
            return skip_option::run;
        } else {
            return skip_option::stop;
        }
    }

    void _p_one_iteration() override {
        switchboard::ptr<const eye_type> eye = _m_eye_subscriber.get_ro_nullable();

        if (eye != nullptr)
        {
            std::vector<double> center0 = ic.Inference(eye->img0);
            std::vector<double> center1 = ic.Inference(eye->img1);
            std::cout << "Done inference at time: " << eye->time.time_since_epoch().count() << std::endl;
            std::cout << "center0 x:" << center0[0] << ", y: " << center0[1] << std::endl;
            std::cout << "center1 x:" << center1[0] << ", y: " << center1[1] << std::endl;

            _m_gaze_publisher.put(_m_gaze_publisher.allocate<gaze_type>(gaze_type{
                eye->time, center0, center1
            }));
        }
    }

private:
    std::string modelPath = "plugins/gaze_inference/best_model_bs1.onnx";
    ImageClassifier ic{modelPath};

    const std::shared_ptr<switchboard>             sb;
    switchboard::reader<eye_type>                  _m_eye_subscriber;
    switchboard::writer<gaze_type>                 _m_gaze_publisher;
};

PLUGIN_MAIN(gaze_inference)
