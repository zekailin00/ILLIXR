#include "image_classifier.h"
// #include <torch/torch.h>
// #include <torch/script.h> 
#include <fstream>
#include <iostream>

#include <algorithm>

#ifdef VERBOSE
/**
 * @brief Print ONNX tensor type
 * https://github.com/microsoft/onnxruntime/blob/rel-1.6.0/include/onnxruntime/core/session/onnxruntime_c_api.h#L93
 * @param os
 * @param type
 * @return std::ostream&
 */
std::ostream& operator<<(std::ostream& os,
                         const ONNXTensorElementDataType& type) {
  switch (type) {
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
      os << "undefined";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
      os << "float";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
      os << "uint8_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
      os << "int8_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
      os << "uint16_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
      os << "int16_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
      os << "int32_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
      os << "int64_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
      os << "std::string";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
      os << "bool";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
      os << "float16";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
      os << "double";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
      os << "uint32_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
      os << "uint64_t";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
      os << "float real + float imaginary";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
      os << "double real + float imaginary";
      break;
    case ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
      os << "bfloat16";
      break;
    default:
      break;
  }

  return os;
}
#endif

std::vector<double> ImageClassifier::getCenter(std::vector<float>& arr) {
  // length check [assert]
  int row_sum = 0;
  int col_sum = 0;
  int count = 0;
  for (int i = 0; i < arr.size(); i++) {
    if (arr[i] == 3) {
      row_sum += i / 640; // row
      col_sum += i % 640; // col
      count++;
    }
  }
  double row_avg = (count > 0) ? row_sum / count : 0; 
  double col_avg = (count > 0) ? col_sum / count : 0; 
  return std::vector<double> {row_avg, col_avg}; // pointers ? lol
}

// Constructor
ImageClassifier::ImageClassifier(const std::string& modelFilepath) {
  /**************** Create ORT environment ******************/
  std::string instanceName{"Image classifier inference"};
  mEnv = std::make_shared<Ort::Env>(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
                                    instanceName.c_str());

  /**************** Create ORT session ******************/
  // Set up options for session
  Ort::SessionOptions sessionOptions;
  // Enable CUDA
  sessionOptions.AppendExecutionProvider_CUDA(OrtCUDAProviderOptions{});
  // Sets graph optimization level (Here, enable all possible optimizations)
  sessionOptions.SetGraphOptimizationLevel(
      GraphOptimizationLevel::ORT_ENABLE_ALL);
  // Create session by loading the onnx model
  mSession = std::make_shared<Ort::Session>(*mEnv, modelFilepath.c_str(),
                                            sessionOptions);

  /**************** Create allocator ******************/
  // Allocator is used to get model information
  Ort::AllocatorWithDefaultOptions allocator;

  /**************** Input info ******************/
  // Get the number of input nodes
  size_t numInputNodes = mSession->GetInputCount();
#ifdef VERBOSE
  std::cout << "******* Model information below *******" << std::endl;
  std::cout << "Number of Input Nodes: " << numInputNodes << std::endl;
#endif

  // Get the name of the input
  // 0 means the first input of the model
  // The example only has one input, so use 0 here
  inPtr = std::move(mSession->GetInputNameAllocated(0, allocator));
  mInputName = inPtr.get();
#ifdef VERBOSE
  std::cout << "Input Name: " << mInputName << std::endl;
#endif

  // Get the type of the input
  // 0 means the first input of the model
  Ort::TypeInfo inputTypeInfo = mSession->GetInputTypeInfo(0);
  auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
  ONNXTensorElementDataType inputType = inputTensorInfo.GetElementType();
#ifdef VERBOSE
  std::cout << "Input Type: " << inputType << std::endl;
#endif

  // Get the shape of the input
  mInputDims = inputTensorInfo.GetShape();
#ifdef VERBOSE
  std::cout << "Input Dimensions: " << mInputDims << std::endl;
#endif

  /**************** Output info ******************/
  // Get the number of output nodes
  size_t numOutputNodes = mSession->GetOutputCount();
#ifdef VERBOSE
  std::cout << "Number of Output Nodes: " << numOutputNodes << std::endl;
#endif

  // Get the name of the output
  // 0 means the first output of the model
  // The example only has one output, so use 0 here
  outPtr = std::move(mSession->GetOutputNameAllocated(0, allocator));
  mOutputName = outPtr.get();
#ifdef VERBOSE
  std::cout << "Output Name: " << mOutputName << std::endl;
#endif

  // Get the type of the output
  // 0 means the first output of the model
  Ort::TypeInfo outputTypeInfo = mSession->GetOutputTypeInfo(0);
  auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
  ONNXTensorElementDataType outputType = outputTensorInfo.GetElementType();
#ifdef VERBOSE
  std::cout << "Output Type: " << outputType << std::endl;
#endif

  // Get the shape of the output
  mOutputDims = outputTensorInfo.GetShape();
#ifdef VERBOSE
  std::cout << "Output Dimensions: " << mOutputDims << std::endl << std::endl;
#endif
}

// Perform inference for a given image
std::vector<double> ImageClassifier::Inference(const cv::Mat& pilimg) {
  // Load an input image
//   cv::Mat pilimg = cv::imread(imageFilepath, cv::IMREAD_GRAYSCALE);
  // cv::Mat imageBGR = cv::imread(imageFilepath, cv::IMREAD_COLOR);

  /**************** Preprocessing ******************/
  // Create input tensor (including size and value) from the loaded input image
#ifdef TIME_PROFILE
  const auto before = clock_time::now();
#endif
  // Compute the product of all input dimension
  size_t inputTensorSize = vectorProduct(mInputDims);
  std::vector<float> inputTensorValues(inputTensorSize);
  // Load the image into the inputTensorValues
  //CreateTensorFromImage(imageBGR, inputTensorValues);
  preProcessImage(pilimg, inputTensorValues);

  // Assign memory for input tensor
  // inputTensors will be used by the Session Run for inference
  std::vector<Ort::Value> inputTensors;
  Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
      OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
  inputTensors.push_back(Ort::Value::CreateTensor<float>(
      memoryInfo, inputTensorValues.data(), inputTensorSize, mInputDims.data(),
      mInputDims.size()));

  // Create output tensor (including size and value)
  size_t outputTensorSize = vectorProduct(mOutputDims);
  std::vector<float> outputTensorValues(outputTensorSize);

  // Assign memory for output tensors
  // outputTensors will be used by the Session Run for inference
  std::vector<Ort::Value> outputTensors;
  outputTensors.push_back(Ort::Value::CreateTensor<float>(
      memoryInfo, outputTensorValues.data(), outputTensorSize,
      mOutputDims.data(), mOutputDims.size()));

#ifdef TIME_PROFILE
  const sec duration = clock_time::now() - before;
  std::cout << "The preprocessing takes " << duration.count() << "s"
            << std::endl;
#endif

  /**************** Inference ******************/
#ifdef TIME_PROFILE
  const auto before1 = clock_time::now();
#endif
  // 1 means number of inputs and outputs
  // InputTensors and OutputTensors, and inputNames and
  // outputNames are used in Session Run
  std::cout << mInputName << std::endl;
  std::cout << mOutputName << std::endl;
  std::vector<const char*> inputNames{mInputName};
  std::vector<const char*> outputNames{mOutputName};
  mSession->Run(Ort::RunOptions{nullptr}, inputNames.data(),
                inputTensors.data(), 1, outputNames.data(),
                outputTensors.data(), 1);

#ifdef TIME_PROFILE
  const sec duration1 = clock_time::now() - before1;
  std::cout << "The inference takes " << duration1.count() << "s" << std::endl;
#endif

  /**************** Postprocessing the output result ******************/
#ifdef TIME_PROFILE
  const auto before2 = clock_time::now();
#endif
  // Get the inference result
  // ************ TODO: process prediction **********
  float* floatarr = outputTensors.front().GetTensorMutableData<float>();
  // Compute the index of the predicted class
  // 10 means number of classes in total
  // int size = 640*400;
  //int idx = std::max_element(floatarr, floatarr + 3) - floatarr;
  // todo: front()?
  int size = outputTensors.front().GetTensorTypeAndShapeInfo().GetElementCount(); // 4096000
  std::ofstream outputFile("output.txt");
  if (outputFile.is_open()) { 
    outputFile << "size: " << size << '\n';
    for (int i = 0; i < size; i++) {
      outputFile << floatarr[i] << '\t'; // write data to the file
    }
    outputFile.close(); // close the file when done
    std::cout << "Data was written to output.txt\n";
  } else {
    std::cerr << "Error opening file\n";
  }
  
  
  std::vector<int64_t> output_dims  = outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();

  // ****printing to understanding dimensions*****
  // std::cout << output_dims << std::endl;
  // std::cout << size << std::endl;
  // std::cout << "elements" << std::endl;
  // for (int i = 0; i < 10; ++i) {
  //     std::cout << floatarr[i] << " ";
  // }
  // std::cout << std::endl;

  // ****3D row major *****
  std::vector<float> predicted_indices;
  std::vector<float> predicted_values;
  int d2 = 400;
  int d3 = 640;
  float c0, c1, c2, c3, maxVal, maxI;
  for (int k = 0; k < d2; k++) { //XDIM
    for (int l = 0; l < d3; l++) { //YDIM
      c0 = floatarr[k*d3+l];
      c1 = floatarr[1*d2*d3 + k*d3 + l];
      c2 = floatarr[2*d2*d3 + k*d3 + l];
      c3 = floatarr[3*d2*d3 + k*d3 + l];
      // if (k < 10) {
      //   // std::cout << "k:" << k << "c0:" << c0 << "c1:" << c1 << "c2:" << c2 << "c3:" << c3 << std::endl;
      // }
      maxVal = std::max({c0, c1, c2, c3});
      if (maxVal == c3) {
        maxI = 3;
      } else if (maxVal == c2) {
        maxI = 2;
      } else if (maxVal == c1) {
        maxI = 1;
      } else {
        maxI = 0;
      }
      predicted_indices.push_back(maxI);
      predicted_values.push_back(maxVal);
    }
  }

  // test pytorch cpp wrapper
  /*
  std::cout << "testing pytorch cpp wrapper";
  auto target_q_T = torch::rand({5, 10, 1}); // todo: update
  auto max_q = torch::max({target_q_T}, 0);
  std::cout << "max: " << std::get<0>(max_q) 
            << "indices: " << std::get<1>(max_q)
            << std::endl;
  */

  // std::cout << "index size" << predicted_indices.size() << std::endl;
  // std::cout << "val size" << predicted_values.size() << std::endl;
  // int start = 158*640;
  // for (int i = start; i < start + 640; ++i) {
  //     // useful
  //     std::cout << predicted_indices[i] << " ";
  //     // std::cout << predicted_values[i] << " ";
  // }

  // float maxIndex = *std::max_element(predicted_indices.begin(), predicted_indices.end());
  // std::cout << std::endl << "maxIndex: " << maxIndex << std::endl;

  std::vector<double> center = getCenter(predicted_indices); // what we want to display

  /*
  def get_predictions(output):
    bs,c,h,w = output.size() // [1,4,400,640]
    values, indices = output.cpu().max(1) // find max index in the row across dim=1
    indices = indices.view(bs,h,w) # bs x h x w [4,400,640]
    return indices
  predict = get_predictions(o_tensor)
  predict[j].cpu().numpy())
  */

#ifdef TIME_PROFILE
  const sec duration2 = clock_time::now() - before2;
  std::cout << "The postprocessing takes " << duration2.count() << "s"
            << std::endl;
#endif

  return center;
}

// Create a tensor from the input image
void ImageClassifier::CreateTensorFromImage(
    const cv::Mat& img, std::vector<float>& inputTensorValues) {
  cv::Mat imageRGB, scaledImage, preprocessedImage;

  /******* Preprocessing *******/
  // Scale image pixels from [0 255] to [-1, 1]
  img.convertTo(scaledImage, CV_32F, 2.0f / 255.0f, -1.0f);
  // Convert HWC to CHW
  cv::dnn::blobFromImage(scaledImage, preprocessedImage);

  // Assign the input image to the input tensor
  inputTensorValues.assign(preprocessedImage.begin<float>(),
                           preprocessedImage.end<float>());
}
//TODO//offload rendering to host
void ImageClassifier::preProcessImage(const cv::Mat& pilimg, std::vector<float>& inputTensorValues) {
  cv::Mat scaledImage, preprocessedImage;
  // Load an input image
  // grayscale (mode “L”) -> https://pillow.readthedocs.io/en/stable/reference/Image.html
  // cv::Mat pilimg = cv::imread(imageFilepath, cv::IMREAD_GRAYSCALE);
  // contrast limited adaptive histogram equalized algorithm
  cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(1.5, cv::Size(8, 8));
  // preprocessing step: fixed gamma value
  cv::Mat table(1, 256, CV_8U); // 1 row, 256 col, 8 bit unsigned integer (0, 255)
  uchar* p = table.ptr();
  for (int i = 0; i < 256; ++i)
      p[i] = cv::saturate_cast<uchar>(std::pow(i / 255.0, 0.8) * 255.0);
  cv::LUT(pilimg, table, pilimg);
  // apply clahe
  clahe->apply(pilimg, pilimg);
  // scaled image -> apply transformation [0, 255] to [0,1] & normalize 
  pilimg.convertTo(scaledImage, CV_32F, 1.0f / 255.0f);
  cv::subtract(scaledImage, cv::Scalar(0.5, 0.5, 0.5), scaledImage);
  cv::divide(scaledImage, cv::Scalar(0.5, 0.5, 0.5), scaledImage);
  // convert image to blob (input to inference)
  cv::dnn::blobFromImage(scaledImage, preprocessedImage);
  // Load the image into the inputTensorValues

  inputTensorValues.assign(preprocessedImage.begin<float>(),
                          preprocessedImage.end<float>());
  // todo: set img name?
}
