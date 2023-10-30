# Example Remote Image Processing Server using RCP (TCP Socket) and Custom Message

In this example we capture images in the client and transmit it to the server, where the processing will be done, and the result image will be transmitted back to client.

To make it transparent and easy to use, we create a custom message
to transmit an OpenCV image (cv::Mat), defined in "image_msg.hpp" -> msg_cvimage.

To improve the transmission speed, we can enable the image compression
by setting the attribute encode to true in our msg_cvimage object. 
The compression is based on OpenCV algorithm for cv::encode, where you can also customize the enconding format (PNG or JPEG).


## Build and Run

To build this example you will need CMAke and OpenCV libraries.

First, install OpenCV libraries:
```bash
sudo apt-get update
sudo apt-get install -y libopencv-dev
```

Then build it:
```bash
mkdir build
cd build
cmake ..
make
```

Run the server in one terminal:
```bash
./image_server
```

Run the client in another terminal:

```bash
./image_client
```
