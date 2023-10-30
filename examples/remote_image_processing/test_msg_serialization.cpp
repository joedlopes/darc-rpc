
#include "image_msg.hpp"

int main(int argc, char* argv[]) {
  cv::VideoCapture cap("/home/vision/Downloads/video.mp4");

  printf("cap open\n");

  msg_cvimage msg_in;
  msg_cvimage msg_out;

  msg_in.encode = true;

  uint8_t* buffer = new uint8_t[2048 * 2048 * 3];
  size_t buffer_size;

  while (true) {
    if (!cap.read(msg_in.img)) {
      cap.set(cv::CAP_PROP_POS_FRAMES, 0);
      break;
    }

    printf("img cap %dx%d\n", msg_in.img.rows, msg_in.img.cols);

    cv::imshow("input", msg_in.img);

    buffer_size = 2048 * 2048 * 4;
    msg_in.serialize(buffer, &buffer_size);

    msg_out.deserialize(buffer, buffer_size);

    cv::imshow("output", msg_out.img);

    if (cv::waitKey(1) == 'q') {
      break;
    }
  }

  return 0;
}
