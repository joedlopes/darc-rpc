
#include "image_msg.hpp"

int main(int argc, char* argv[]) {
  const uint16_t M_SOBEL = 0x0000;
  const uint16_t M_INVERT = 0x0001;
  const uint16_t M_BLUR = 0x0002;

  dc::socket_requirements::init();
  dc::rpc_client client("0.0.0.0", 31311);

  if (!client.connect()) {
    fprintf(stderr, "[image_client] fail to connect to image server\n");
    return 1;
  }

  // set video path name
  cv::VideoCapture cap("/home/vision/Downloads/video.mp4");

  while (true) {
    msg_cvimage msg_in;
    msg_cvimage msg_out;
    if (!cap.read(msg_in.img)) {
      cap.set(cv::CAP_PROP_POS_FRAMES, 0);
      continue;
      ;
    }

    msg_in.encode = false;

    if (!client.execute(M_SOBEL, &msg_in, &msg_out)) {
      fprintf(stderr, "[image_client] fail to execute remote method\n");
      break;
    }

    msg_in.img.release();

    msg_out.img.copyTo(msg_in.img);

    if (!client.execute(M_BLUR, &msg_in, &msg_out)) {
      fprintf(stderr, "[image_client] fail to execute remote method\n");
      break;
    }

    msg_out.img.copyTo(msg_in.img);

    if (!client.execute(M_INVERT, &msg_in, &msg_out)) {
      fprintf(stderr, "[image_client] fail to execute remote method\n");
      break;
    }

    cv::imshow("output", msg_out.img);
    if (cv::waitKey(1) == 'q') {
      break;
    }

    msg_out.img.release();
    msg_in.img.release();
  }

  return 0;
}
