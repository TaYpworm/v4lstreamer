#include "v4lstreamer.h"
#include "IOException.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#define SAT(c) if (c & (~255)) { if (c < 0) c = 0; else c = 255; }
#define CLEAR(x) memset (&(x), 0, sizeof (x))

V4LStreamer::V4LStreamer(ioMethod ioMeth, string devName, bool RGBval, int width, int height, int channel, int numBuffers, unsigned int pixelFormat, v4l2_field field, v4l2_std_id std) {
    streaming = false;
    io = ioMeth;
    deviceName = devName;
    RGB = RGBval;
    cameraFD = -1;
    FD_ZERO(&fds);
    this->numBuffers = numBuffers;
    CLEAR(cap);
    CLEAR(cropcap);
    CLEAR(crop);
    CLEAR(fmt);
    CLEAR(input);
   
    initDevice(height, width, channel, pixelFormat, field, std);
}

V4LStreamer::~V4LStreamer() {
    if (streaming) {
        stopCapture();
    } else {
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
            free (buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < numBuffers; ++i)
                if (-1 == munmap (buffers[i].start, buffers[i].length))
                    throw IOException("munmap");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < numBuffers; ++i)
                free (buffers[i].start);
            break;
        }   

    free (buffers);  
    }
}

void V4LStreamer::setRGB(bool RGBval) {
    RGB = RGBval;
}

bool V4LStreamer::getRGB() {
    return RGB;
}

void V4LStreamer::setResolution(int width, int height) {
    if (!streaming) {
        unsigned int min;
        fmt.fmt.pix.width = width; 
        fmt.fmt.pix.height = height;
    
        if (-1 == xioctl (cameraFD, VIDIOC_S_FMT, &fmt))
            throw IOException("VIDIOC_S_FMT: Failed to set resolution");

        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
            fmt.fmt.pix.sizeimage = min;
    }
}

void V4LStreamer::getResolution(int &width, int &height) {
    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;
}

void V4LStreamer::setChannel(int channel) {
    if (!streaming) {
        input.index = channel;
        //if (-1 == xioctl (cameraFD, VIDIOC_ENUMINPUT, &input)) 
        //    throw IOException("VIDIOC_ENUMINPUT: Unable to set channel");
        if (-1 == xioctl (cameraFD, VIDIOC_S_INPUT, &channel)) 
            throw IOException("VIDIOC_S_INPUT: Unable to set channel");
    }
}

int V4LStreamer::getChannel() {
    return input.index;
}

void V4LStreamer::setStd(v4l2_std_id std) {
    memset (&input, 0, sizeof (input));

    if (-1 == ioctl (cameraFD, VIDIOC_G_INPUT, &input.index))
        throw IOException("VIDIOC_G_INPUT query error");
    if (-1 == ioctl (cameraFD, VIDIOC_ENUMINPUT, &input))
        throw IOException("VIDIOC_ENUM_INPUT query error");
    if (0 == (input.std & std))
        throw IOException("Unsupported video standard");
    if (-1 == ioctl (cameraFD, VIDIOC_S_STD, &std)) 
        throw IOException("Standard configuration error");
}

void V4LStreamer::setPixelFormat(unsigned int format) {
    if (!streaming) {
        fmt.fmt.pix.pixelformat = format;
        if (-1 == xioctl (cameraFD, VIDIOC_S_FMT, &fmt))
            throw IOException("VIDIOC_S_FMT: Unable to set pixel format");
    }
}

int V4LStreamer::getPixelFormat() {
    return fmt.fmt.pix.pixelformat;
}

void V4LStreamer::setField(v4l2_field field) {
    if (!streaming) {
        fmt.fmt.pix.field = field;
        if (-1 == xioctl (cameraFD, VIDIOC_S_FMT, &fmt))
            throw IOException("VIDIOC_ENUMINPUT: Unable to set field");
    }
}

v4l2_field V4LStreamer::getField() {
    return fmt.fmt.pix.field;
}

//void V4LStreamer::setNumBuffers(int numBuffers) {
//    this->numBuffers = numBuffers;
//}

int V4LStreamer::getNumbuffers() {
    return numBuffers;
}


int V4LStreamer::getImageSize() {
    return fmt.fmt.pix.sizeimage;
}

int V4LStreamer::getBytesPerLine() {
    return fmt.fmt.pix.bytesperline;
}

void V4LStreamer::startCapture() {
    unsigned int i;
    enum v4l2_buf_type type;

    switch (io) {
    case IO_METHOD_READ:
        /* Nothing to do. */
        break;

    case IO_METHOD_MMAP:
        for (i = 0; i < numBuffers; ++i) {
            struct v4l2_buffer buf;

            CLEAR (buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = i;

            if (-1 == xioctl (cameraFD, VIDIOC_QBUF, &buf))
                throw IOException("VIDIOC_QBUF error");
        }
                
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (-1 == xioctl (cameraFD, VIDIOC_STREAMON, &type))
            throw IOException("VIDIOC_STREAMON error");

        break;

    case IO_METHOD_USERPTR:
        for (i = 0; i < numBuffers; ++i) {
            struct v4l2_buffer buf;

            CLEAR (buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_USERPTR;
            buf.index       = i;
            buf.m.userptr   = (unsigned long) buffers[i].start;
            buf.length      = buffers[i].length;

            if (-1 == xioctl (cameraFD, VIDIOC_QBUF, &buf))
                throw IOException("VIDIOC_QBUF error");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (cameraFD, VIDIOC_STREAMON, &type))
                throw IOException("VIDIOC_STREAMON error");

            break;
        }

    streaming = true;
}

void V4LStreamer::stopCapture() {
    enum v4l2_buf_type type;

    switch (io) {
    case IO_METHOD_READ:
        /* Nothing to do. */
        break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (-1 == xioctl (cameraFD, VIDIOC_STREAMOFF, &type))
            throw IOException("VIDIOC_STREAMOFF");

        break;
    }

    streaming = false;
}

int V4LStreamer::readFrame(void *frame, int &bytesRead) {
    int retval;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    retval = select(cameraFD+1, &fds, NULL, NULL, &tv);
    if (retval == -1) {
        if (errno != EINTR) 
            throw IOException("Select error");
    }

    if (retval == 0) {
        throw IOException("Select timeout");
    }
    
    if (RGB) {
        readRGB(frame, bytesRead);
    } else {
        readRaw(frame, bytesRead);
    }
}

void V4LStreamer::initDevice(int height, int width, int channel, unsigned int pixelFormat, v4l2_field field, v4l2_std_id std) {
    unsigned int min;
    struct stat st; 

    if (-1 == stat (deviceName.c_str(), &st)) {
        char *message = new char[256];
        sprintf(message, "Cannot identify device '%s': %d, %s", deviceName.c_str(), errno, strerror(errno));
        throw IOException(message);
    }

    if (!S_ISCHR(st.st_mode)) {
        char *message = new char[256];
        sprintf(message, "%s is not a character device", deviceName.c_str());
        throw IOException(message);
    }

    cameraFD = open(deviceName.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == cameraFD) {
        char *message = new char[256];
        sprintf(message, "Cannot open '%s': %d, %s", deviceName.c_str(), errno, strerror(errno));
        throw IOException(message);
    }
    
    if (-1 == xioctl (cameraFD, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            char *message = new char[256];
            sprintf(message, "%s is not a V4L2 device", deviceName.c_str());
            throw IOException(message);
        } else {
            throw IOException("VIDIOC_QUERYCAP error");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        char *message = new char[256];
        sprintf(message, "%s is not a video capture device", deviceName.c_str());
        throw IOException(message);
    }

    switch (io) {
    case IO_METHOD_READ:
        if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
            char *message = new char[256];
            sprintf(message, "%s does not support read i/o", deviceName.c_str());
            throw IOException(message);
        }
        break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            char *message = new char[256];
            sprintf(message, "%s does not support streaming i/o", deviceName.c_str());
            throw IOException(message);
        }

        break;
    }

    /* Select video input, video standard and tune here. */

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (cameraFD, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl (cameraFD, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
            case EINVAL:
                /* Cropping not supported. */
                break;
            default:
                /* Errors ignored. */
                break;
            }
        }
    } else {        
        /* Errors ignored. */
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setPixelFormat(pixelFormat);
    setField(field);
    setResolution(width, height);
    setChannel(channel);
    if (std > 0) {
    	setStd(std);
    }

    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    initIO();
}

void V4LStreamer::initVars() {
    streaming = false;
    cameraFD = -1;
    numBuffers = 4;
    io = IO_METHOD_MMAP;
    deviceName = "/dev/video";
}

void V4LStreamer::initIO() {
    switch (io) {
    case IO_METHOD_READ:
        initRead();
        break;

    case IO_METHOD_MMAP:
        initMMAP();
        break;

    case IO_METHOD_USERPTR:
        initUserPtr();
        break;
    }

    FD_SET(cameraFD, &fds);
}

void V4LStreamer::initRead() {
    unsigned int bufferSize = fmt.fmt.pix.sizeimage;
    buffers = (buffer*)calloc (1, sizeof (*buffers));

    if (!buffers)
        throw bad_alloc();

    buffers[0].length = bufferSize;
    buffers[0].start = malloc(bufferSize);

    if (!buffers[0].start) 
        throw bad_alloc();
}

void V4LStreamer::initMMAP() {
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = numBuffers;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (cameraFD, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            char *message = new char[256];
            sprintf(message, "%s does not support memory mapping", deviceName.c_str());
            throw IOException(message);
        } else {
            throw IOException("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        char *message = new char[256];
        sprintf(message, "Insufficient buffer memory on %s", deviceName.c_str());
        throw IOException(message);
    }

    buffers = (buffer*)calloc (req.count, sizeof (*buffers));

    if (!buffers) {
        throw bad_alloc();
    }

    for (int n_buffers = 0; n_buffers < (int)req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (cameraFD, VIDIOC_QUERYBUF, &buf))
            throw IOException("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                cameraFD, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            throw IOException("MMAP failed");
    }
}

void V4LStreamer::initUserPtr() {
    unsigned int bufferSize = fmt.fmt.pix.sizeimage;
    struct v4l2_requestbuffers req;
    unsigned int pageSize;

    pageSize = getpagesize ();
    bufferSize = (bufferSize + pageSize - 1) & ~(pageSize - 1);

    CLEAR (req);

    req.count               = numBuffers;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (cameraFD, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            char *message = new char[256];
            sprintf(message, "%s does not support user pointer i/o", deviceName.c_str());
            throw IOException(message);
        } else {
            throw IOException("VIDIOC_REQBUFS");
        }
    }

    buffers = (buffer*)calloc (req.count, sizeof (*buffers));

    if (!buffers) {
        throw bad_alloc();
    }

    for (int n_buffers = 0; n_buffers < (int)req.count; ++n_buffers) {
        buffers[n_buffers].length = bufferSize;
        buffers[n_buffers].start = memalign (/* boundary */ pageSize, bufferSize);

        if (!buffers[n_buffers].start) {
            throw bad_alloc();
        }
    }
}

int V4LStreamer::xioctl(int fd, int request, void *arg) {
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    
    return r;
}

void V4LStreamer::YUYVTORGB24(int width, int height, unsigned char *src, unsigned char *dst) {
    unsigned char *s;
    unsigned char *d;
    int l, c;
    int r, g, b, cr, cg, cb, y1, y2;

    l = height;
    s = src;
    d = dst;
    while (l--) {
        c = width >> 1;
        while (c--) {
            y1 = *s++;
            cb = ((*s - 128) * 454) >> 8;
            cg = (*s++ - 128) * 88;
            y2 = *s++;
            cr = ((*s - 128) * 359) >> 8;
            cg = (cg + (*s++ - 128) * 183) >> 8;

            r = y1 + cr;
            b = y1 + cb;
            g = y1 - cg;
            SAT(r);
            SAT(g);
            SAT(b);

            *d++ = b;
            *d++ = g;
            *d++ = r;

            r = y2 + cr;
            b = y2 + cb;
            g = y2 - cg;
            SAT(r);
            SAT(g);
            SAT(b);

            *d++ = b;
            *d++ = g;
            *d++ = r;
        }
    }
}

int V4LStreamer::readRaw(void *frame, int &bytesRead) {
    if (streaming) {
        struct v4l2_buffer buf;
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
            if (-1 == read (cameraFD, buffers[0].start, buffers[0].length)) {
                switch (errno) {
                case EAGAIN:
                    return 0;

                case EIO:
                    /* Could ignore EIO, see spec. */
                    /* fall through */

                default:
                    throw IOException("Read error");
                }
            }
        
            memcpy(frame, buffers[0].start, fmt.fmt.pix.sizeimage);
            bytesRead = buffers[0].length;
            break;

        case IO_METHOD_MMAP:
            CLEAR (buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl (cameraFD, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                case EAGAIN:
                    return 0;

                case EIO:
                    /* Could ignore EIO, see spec. */
                    /* fall through */

                default:
                    throw IOException("VIDIOC_DQBUF");
                }
            }

            if ((int)buf.index > numBuffers)
                throw IOException("Invalid buffer number");
       
            memcpy(frame, buffers[buf.index].start, fmt.fmt.pix.sizeimage);
            bytesRead = buf.length;

            if (-1 == xioctl (cameraFD, VIDIOC_QBUF, &buf))
                throw IOException("VIDIOC_QBUF");

            break;

        case IO_METHOD_USERPTR:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (-1 == xioctl (cameraFD, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                case EAGAIN:
                    return 0;

                case EIO:
                    /* Could ignore EIO, see spec. */
                    /* fall through */

                default:
                    throw IOException("VIDIOC_DQBUF");
                }
            }

            for (i = 0; (int)i < numBuffers; ++i)
                if (buf.m.userptr == (unsigned long) buffers[i].start && buf.length == buffers[i].length)
                    break;
        
            if ((int)i > numBuffers)
                throw IOException("Invalid buffer number");

            memcpy(frame, (void *) buf.m.userptr, fmt.fmt.pix.sizeimage);
            bytesRead = buf.length;

            if (-1 == xioctl (cameraFD, VIDIOC_QBUF, &buf))
                throw IOException("VIDIOC_QBUF");

            break;
        }

        return 1;
    }
    return 0;

}

int V4LStreamer::readRGB(void *frame, int &bytesRead) {
    if (streaming) {
        int retval;
        void *tmp = malloc(fmt.fmt.pix.sizeimage);
        retval = readRaw(tmp, bytesRead);

        switch(fmt.fmt.pix.pixelformat) {
        case V4L2_PIX_FMT_YUYV:
            YUYVTORGB24(fmt.fmt.pix.width, fmt.fmt.pix.height, (unsigned char*) tmp, (unsigned char*) frame);
            break;
        
        default:
            throw IOException("Unsupported pixel format conversion");
        };
        
        bytesRead = fmt.fmt.pix.width * fmt.fmt.pix.height * 3;
	free(tmp);
        return retval;
    }
    return 0;
}

