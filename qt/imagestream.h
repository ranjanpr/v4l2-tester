#ifndef IMAGESTREAM_HPP
#define IMAGESTREAM_HPP

#include <QMutex>

class ImageStream
{
public:
    ImageStream(int width, int height);
    ~ImageStream();
    uchar *getFrontImage();
    uchar *getBackImage();
    void swapImage();
    bool isUpdated();
    void decUpdated();
    void lockFrontImage();
    void unlockFrontImage();
    int getWidth();
    int getHeight();

    void yuv2rgb(const uchar *yuv, int yw, int yh);
    void yuyv2rgb(const uchar *yuv, int yw, int yh);
    void uyvy2rgb(const uchar *yuv, int yw, int yh);

private:
	uchar *data[2];
    int front_index;
    int updated;
    int m_width;
    int m_height;
    QMutex mutex;
};

#endif // IMAGESTREAM_HPP
