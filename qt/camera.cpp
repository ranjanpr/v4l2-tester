#include "camera.h"
#include "imagestream.h"
#include "camera_texture.h"
#include "camera_texture.h"
#include "yuv2rgb_material.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <QDebug>
#include <QSGGeometryNode>

char *Camera::CAPTURE_DEVICE = 0;

Camera::Camera(QObject *parent)
    : QThread(parent), m_image(0), m_texture(0),
      frame_count(0), frame_devisor(1), m_running(false),
      m_node(0)
{
    qDebug()<<"Camera::Camera";
    videodev.fd = -1;
}

Camera::~Camera()
{
    qDebug()<<"Camera::~Camera";
    closeCapture();

    if (m_texture)
        delete m_texture;
    if (m_image)
        delete m_image;
}

QSGGeometryNode *Camera::createNode()
{
    qDebug()<<"Camera::createNode";

    QSGGeometry *geometry;

    if (!m_texture)
        m_texture = new CameraTexture(m_image->getWidth(), m_image->getHeight());

    if (!m_node) {
        m_node = new QSGGeometryNode;
        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        geometry->setDrawingMode(GL_TRIANGLE_STRIP);
        m_node->setGeometry(geometry);
        m_node->setFlag(QSGNode::OwnsGeometry);

        YUV2RGBMaterial *material = new YUV2RGBMaterial;
        material->setTexture(m_texture);
        m_node->setMaterial(material);
        m_node->setFlag(QSGNode::OwnsMaterial);
    }

    return m_node;
}

void Camera::updateGeometry(qreal x, qreal y, qreal width, qreal height)
{
    qDebug()<<"Camera::updateGeometry";

    QSGGeometry::TexturedPoint2D *vertices = m_node->geometry()->vertexDataAsTexturedPoint2D();

    vertices[0].x = x;
    vertices[0].y = y + height;
    vertices[0].tx = 0;
    vertices[0].ty = 1;

    vertices[1].x = x;
    vertices[1].y = y;
    vertices[1].tx = 0;
    vertices[1].ty = 0;

    vertices[2].x = x + width;
    vertices[2].y = y + height;
    vertices[2].tx = 1;
    vertices[2].ty = 1;

    vertices[3].x = x + width;
    vertices[3].y = y;
    vertices[3].tx = 1;
    vertices[3].ty = 0;
}

void Camera::startStream()
{
    qDebug()<<"Camera::startStream";

    m_running = true;
    m_wait.wakeAll();
}

void Camera::stopStream()
{
    qDebug()<<"Camera::stopStream";

    m_running = false;
}

void Camera::run()
{
    qDebug()<<"Camera::run";

    if (initCapture() < 0)
        return;

    if (startCapture() < 0)
        return;

    while (1) {
        if (captureFrame() < 0)
            break;
    }

    stopCapture();
    closeCapture();
}

void Camera::textureProcess(const uchar *data, int width, int height)
{
    qDebug()<<"Camera::textureProcess";

    Q_UNUSED(data);
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void Camera::updateTexture(const uchar *data, int width, int height)
{
    qDebug()<<"Camera::updateTexture";

    if (!m_running)
        return;

    textureProcess(data, width, height);
    m_image->swapImage();
    emit imageChanged();
}

void Camera::updateMaterial()
{
    qDebug()<<"Camera::updateMaterial";

    if(m_texture)
    {
        m_texture->updateFrame(m_image->getFrontImage());
    }
    else
    {
        qDebug()<<"Camera::updateMaterial m_texture NULL";
    }
}
void Camera::vidioc_enuminput(int fd)
{
    qDebug()<<"Camera::vidioc_enuminput";
    int err;
    struct v4l2_input input;
    memset(&input, 0, sizeof(input));
    input.index = 0;
    while ((err = ioctl(fd, VIDIOC_ENUMINPUT, &input)) == 0) {
        qDebug() << "input name =" << (char *)input.name
                 << " type =" << input.type
                 << " status =" << input.status
                 << " std =" << input.std;
        input.index++;
    }
}

int Camera::initCapture()
{
    qDebug()<<"Camera::initCapture";
    if (videodev.fd > 0)
        return 0;

    int err;
    int fd = open(CAPTURE_DEVICE, O_RDWR);
    if (fd < 0) {
        qWarning() << "open /dev/video0 fail " << fd;
        return fd;
    }
    videodev.fd = fd;

    struct v4l2_capability cap;
    if ((err = ioctl(fd, VIDIOC_QUERYCAP, &cap)) < 0) {
        qWarning() << "VIDIOC_QUERYCAP error " << err;
        goto err1;
    }
    qDebug() << "card =" << (char *)cap.card
             << " driver =" << (char *)cap.driver
             << " bus =" << (char *)cap.bus_info;

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        qDebug() << "/dev/video0: Capable off capture";
    else {
        qWarning() << "/dev/video0: Not capable of capture";
        goto err1;
    }

    if (cap.capabilities & V4L2_CAP_STREAMING)
        qDebug() << "/dev/video0: Capable of streaming";
    else {
        qWarning() << "/dev/video0: Not capable of streaming";
        goto err1;
    }

    if ((err = subInitCapture()) < 0)
        goto err1;

    struct v4l2_requestbuffers reqbuf;
    reqbuf.count = 5;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if ((err = ioctl(fd, VIDIOC_REQBUFS, &reqbuf)) < 0) {
        qWarning() << "Cannot allocate memory";
        goto err1;
    }
    videodev.numbuffer = reqbuf.count;
    qDebug() << "buffer actually allocated" << reqbuf.count;

    uint i;
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    for (i = 0; i < reqbuf.count; i++) {
        buf.type = reqbuf.type;
        buf.index = i;
        buf.memory = reqbuf.memory;
        err = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        Q_ASSERT(err == 0);

        videodev.buff_info[i].length = buf.length;
        videodev.buff_info[i].index = i;
        videodev.buff_info[i].start =
                (uchar *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        Q_ASSERT(videodev.buff_info[i].start != MAP_FAILED);

        memset((void *) videodev.buff_info[i].start, 0x80,
               videodev.buff_info[i].length);

        err = ioctl(fd, VIDIOC_QBUF, &buf);
        Q_ASSERT(err == 0);
    }

    return 0;

err1:
    close(fd);
    return err;
}

int Camera::startCapture()
{
    qDebug()<<"Camera::startCapture";
    int a, ret;

    /* Start Streaming. on capture device */
    a = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videodev.fd, VIDIOC_STREAMON, &a);
    if (ret < 0) {
        qDebug() << "capture VIDIOC_STREAMON error fd=" << videodev.fd;
        return ret;
    }
    qDebug() << "Stream on...";

    return 0;
}

int Camera::captureFrame()
{
    qDebug()<<"Camera::captureFrame";
    int ret;
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;

    /* Dequeue capture buffer */
    ret = ioctl(videodev.fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        qDebug() << "Cap VIDIOC_DQBUF";
        return ret;
    }

    if (frame_count++ % frame_devisor == 0)
    {
        int outfd = open("out.img", O_RDWR|O_CREAT);
        if(outfd<0)
        {
            qDebug()<<"OUT image error";
        }

        write(outfd, videodev.buff_info[buf.index].start, buf.bytesused);
        close(outfd);

        qDebug()<<"OUT IMAGE DONE";

        exit(0);
//        updateTexture(
//                (uchar *)videodev.buff_info[buf.index].start,
//                videodev.cap_width,
//                videodev.cap_height);
    }
    ret = ioctl(videodev.fd, VIDIOC_QBUF, &buf);
    if (ret < 0) {
        qDebug() << "Cap VIDIOC_QBUF";
        return ret;
    }

    return 0;
}

int Camera::stopCapture()
{
    qDebug()<<"Camera::stopCapture";
    int a, ret;

    qDebug() << "Stream off!!\n";

    a = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videodev.fd, VIDIOC_STREAMOFF, &a);
    if (ret < 0) {
        qDebug() << "VIDIOC_STREAMOFF";
        return ret;
    }

    return 0;
}

void Camera::closeCapture()
{
    qDebug()<<"Camera::closeCapture";
    int i;
    struct buf_info *buff_info;

    /* Un-map the buffers */
    for (i = 0; i < CAPTURE_MAX_BUFFER; i++){
        buff_info = &videodev.buff_info[i];
        if (buff_info->start) {
            munmap(buff_info->start, buff_info->length);
            buff_info->start = NULL;
        }
    }

    if (videodev.fd >= 0) {
        close(videodev.fd);
        videodev.fd = -1;
    }
}
