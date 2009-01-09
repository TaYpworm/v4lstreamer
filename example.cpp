#include "v4lstreamer.h"

#include <cstdio>
#include <string>
#include <unistd.h>


int main(int argc, char **argv) {
    int c, bytesRead;
    int width, height, imageSize, numImages=100;
    unsigned char *imgData;
    string device;
    V4LStreamer *cam;
    

    opterr = 0;

    while ((c = getopt(argc, argv, "d:x:y:h")) != -1) {
        switch (c) {
        case 'd':
            device = (int)optarg;
            break;
        case 'x':
            width = (int)optarg;
            break;
        case 'y':
            height = (int)optarg;
            break;
        case 'h':
        default :
            printf("Useage:  example -d <device name> -x <image width> -y <image height>\n");
            return 1;
        }
    }

    cam = new V4LStreamer(IO_METHOD_MMAP, device, true, width, height, 1, 4, V4L2_PIX_FMT_YUYV, V4L2_FIELD_INTERLACED, V4L2_STD_NTSC_M);

    imageSize = cam->getImageSize();
    imgData = new unsigned char[imageSize];

    cam->startCapture();

    for (int i=0; i < numImages; i++) {
        cam->readFrame(imgData, bytesRead);

        printf("Read image number %d with size %d\n", i, bytesRead);
    }

    cam->stopCapture();

    return 0;
}

