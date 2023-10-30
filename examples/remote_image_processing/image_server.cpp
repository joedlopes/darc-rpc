#include "image_msg.hpp"

int main(int argc, char* argv[]) {
  dc::socket_requirements::init();

  dc::rpc_server<msg_cvimage, msg_cvimage> server("0.0.0.0", 31311);

  const uint16_t M_SOBEL = 0x0000;
  server.register_method(M_SOBEL, [](msg_cvimage* input, msg_cvimage* output) {
    cv::Mat gray, sobelX, sobelY, sobelCombined;

    cv::cvtColor(input->img, gray, cv::COLOR_BGR2GRAY);

    cv::Sobel(gray, sobelX, CV_16S, 1, 0);
    cv::Sobel(gray, sobelY, CV_16S, 0, 1);

    cv::convertScaleAbs(sobelX, sobelX);
    cv::convertScaleAbs(sobelY, sobelY);

    cv::addWeighted(sobelX, 0.5, sobelY, 0.5, 0, output->img);
  });

  const uint16_t M_INVERT = 0x0001;
  server.register_method(M_INVERT, [](msg_cvimage* input, msg_cvimage* output) {
    output->img = cv::Scalar::all(255) - input->img;
  });

  const uint16_t M_BLUR = 0x0002;
  server.register_method(M_BLUR, [](msg_cvimage* input, msg_cvimage* output) {
    cv::GaussianBlur(input->img, output->img, cv::Size(15, 15), 2.0, 2.0);
  });

  server.run();

  return 0;
}
