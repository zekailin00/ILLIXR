#pragma once

#include "illixr/csv_iterator.hpp"
#include "illixr/data_format.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include <chrono>
#include <shared_mutex>
#include <thread>

#include "dirent.h"


typedef unsigned long long ullong;

/*
 * Uncommenting this preprocessor macro makes the offline_eye load each data from the disk as it is needed.
 * Otherwise, we load all of them at the beginning, hold them in memory, and drop them in the queue as needed.
 * Lazy loading has an artificial negative impact on performance which is absent from an online-sensor system.
 * Eager loading deteriorates the startup time and uses more memory.
 */
//#define LAZY

class lazy_load_image {
public:
    lazy_load_image() { }

    lazy_load_image(std::string path)
        : _m_path(std::move(path)) {
#ifndef LAZY
        _m_mat = cv::imread(_m_path, cv::IMREAD_GRAYSCALE);
#endif
    }

    [[nodiscard]] cv::Mat load() const {
#ifdef LAZY
        cv::Mat _m_mat = cv::imread(_m_path, cv::IMREAD_GRAYSCALE);
    #error "Linux scheduler cannot interrupt IO work, so lazy-loading is unadvisable."
#endif
        // assert(!_m_mat.empty());
        return _m_mat;
    }

private:
    std::string _m_path;
    cv::Mat     _m_mat;
};

typedef struct {
    lazy_load_image eye0;
    lazy_load_image eye1;
} sensor_types;

/**
 * @brief Get all the image filenames in a specified directory
 * @param img_dir: the input directory
 * @param img_names: the vector storing all the image filenames
 */
void getAllImageFiles(const std::string &img_dir,
                      std::vector<std::string> &img_names) {
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(img_dir.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      std::string filename(ent->d_name);
      if (filename == "." || filename == "..") continue;
      img_names.push_back(filename);
    }
    closedir(dir);
  } else {
    // Failed to open directory
    perror("");
    exit(EXIT_FAILURE);
  }
}

static std::map<ullong, sensor_types> load_data() {
    std::map<ullong, sensor_types> data;

    std::string img_dir("plugins/offline_eye/images");
    std::vector<std::string> img_names;
    getAllImageFiles(img_dir, img_names);

    const char* illixr_data_c_str = std::getenv("ILLIXR_DATA");
    if (!illixr_data_c_str) {
        spdlog::get("illixr")->error("[offline_eye] Please define ILLIXR_DATA");
        ILLIXR::abort();
    }
    std::string illixr_data = std::string{illixr_data_c_str};

    const std::string eye0_subpath = "/cam0/data.csv";
    std::ifstream     eye0_file{illixr_data + eye0_subpath};
    int fileIndex = 0;
    int totalFiles = 155;
    if (!eye0_file.good()) {
        spdlog::get("illixr")->error("[offline_eye] ${ILLIXR_DATA} {0} ({1}{0}) is not a good path", eye0_subpath, illixr_data);
        ILLIXR::abort();
    }
    for (CSVIterator row{eye0_file, 1}; row != CSVIterator{}; ++row) {
        ullong t     = std::stoull(row[0]);
        data[t].eye0 = {"plugins/offline_eye/images/" + img_names[fileIndex++ % totalFiles]};
    }

    const std::string eye1_subpath = "/cam1/data.csv";
    std::ifstream     eye1_file{illixr_data + eye1_subpath};
    fileIndex = 0;
    if (!eye1_file.good()) {
        spdlog::get("illixr")->error("[offline_eye] ${ILLIXR_DATA} {0} ({1}{0}) is not a good path", eye1_subpath, illixr_data);
        ILLIXR::abort();
    }
    for (CSVIterator row{eye1_file, 1}; row != CSVIterator{}; ++row) {
        ullong      t     = std::stoull(row[0]);
        data[t].eye1      = {"plugins/offline_eye/images/" + img_names[fileIndex++ % totalFiles]};
    }

    return data;
}
