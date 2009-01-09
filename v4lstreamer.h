#ifndef V4LSTREAMER
#define V4LSTREAMER

#include <string>
#include <linux/videodev2.h>
#include <sys/select.h>

using namespace std;


enum ioMethod {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR
};

/*
enum pixelFormat {
    GREY = V4L2_PIX_FMT_GREY,
    YUYV = V4L2_PIX_FMT_YUYV
};
*/

struct buffer {
    void *start;
    size_t length;
};

class V4LStreamer {
public:
    V4LStreamer(ioMethod io, string deviceName, bool RGB, int width, int height, int channel, int numBuffers, unsigned int pixelFormat, v4l2_field field, v4l2_std_id std);
    ~V4LStreamer();
    void setRGB(bool RGBval);
    bool getRGB();
    void setResolution(int width, int height);
    void getResolution(int &width, int &height);
    void setChannel(int channel);
    int getChannel();
    void setStd(v4l2_std_id std);
    v4l2_std_id getStd();
    void setPixelFormat(unsigned int format);
    int getPixelFormat();
    void setField(v4l2_field field);
    v4l2_field getField();
    //void setNumBuffers(int numBuffers);
    int getNumbuffers();
    int getImageSize();
    int getBytesPerLine();
    void startCapture();
    void stopCapture();
    int readFrame(void *frame, int &bytesRead);

private:
    bool streaming;
    int cameraFD;
    bool RGB;
    int numBuffers;
    string deviceName;
    fd_set fds;
    ioMethod io;
    struct buffer *buffers;
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    struct v4l2_input input;

private:
    void initDevice(int height, int width, int channel, unsigned int pixelFormat, v4l2_field field, v4l2_std_id std);
    void initVars();
    void initIO();
    void initRead();
    void initMMAP();
    void initUserPtr();
    int xioctl(int fd, int request, void *arg);
    void YUYVTORGB24(int width, int height, unsigned char *src, unsigned char *dst);
    int readRaw(void *frame, int &bytesRead);
    int readRGB(void *frame, int &bytesRead);
};

#endif

