#include "imagestream.h"
#include <QDebug>

ImageStream::ImageStream(int width, int height)
    : front_index(0), updated(false), m_width(width), m_height(height)
{
    qDebug()<<"ImageStream::ImageStream";
    for (int i = 0; i < 2; i++)
		data[i] = new uchar[width * height * 3];
}

ImageStream::~ImageStream()
{
    qDebug()<<"ImageStream::~ImageStream";
    for (int i = 0; i < 2; i++)
		delete data[i];
}

uchar *ImageStream::getFrontImage()
{
    qDebug()<<"ImageStream::getFrontImage";
    return data[front_index];
}

uchar *ImageStream::getBackImage()
{
    qDebug()<<"ImageStream::getBackImage";
    return data[(front_index + 1) % 2];
}

void ImageStream::yuv2rgb(const uchar *yuv, int yw, int yh)
{
    qDebug()<<"ImageStream::yuv2rgb";
    int i, j;
    int w = m_width, h = m_height;
	uchar *rgb = getBackImage();

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
			rgb[i * w * 3 + j * 3 + 2] = yuv[yw * yh + i * yw + j * 2 + 1];
			rgb[i * w * 3 + j * 3 + 1] = yuv[yw * yh + i * yw + j * 2];
			rgb[i * w * 3 + j * 3 + 0] = yuv[i * yw * 2 + j * 2];
        }
    }
}

void ImageStream::yuyv2rgb(const uchar *yuv, int yw, int yh)
{
    qDebug()<<"ImageStream::yuyv2rgb";
    uchar *rgb = getBackImage();

    for (int i = 0; i < yw * yh; i++) {
        rgb[i * 3 + 0] = yuv[i * 2];
        rgb[i * 3 + 1] = yuv[i * 2 + (i & 1 ? -1 : 1)];
        rgb[i * 3 + 2] = yuv[i * 2 + (i & 1 ? 1 : 3)];
    }
}

void ImageStream::uyvy2rgb(const uchar *yuv, int yw, int yh)
{
    qDebug()<<"ImageStream::uyvy2rgb";
    uchar *rgb = getBackImage();

    for (int i = 0; i < yw * yh; i++) {
        rgb[i * 3 + 0] = yuv[i * 2 + 1];
        rgb[i * 3 + 1] = yuv[i * 2 + (i & 1 ? -2 : 0)];
        rgb[i * 3 + 2] = yuv[i * 2 + (i & 1 ? 0 : 2)];
    }
}

void ImageStream::swapImage()
{
    qDebug()<<"ImageStream::swapImage";
    mutex.lock();
    front_index = (front_index + 1) % 2;
    updated = 4;
    mutex.unlock();
}

bool ImageStream::isUpdated() { return updated; }

void ImageStream::decUpdated() { updated--; }

void ImageStream::lockFrontImage() { mutex.lock(); }

void ImageStream::unlockFrontImage() { mutex.unlock(); }

int ImageStream::getWidth() { return m_width; }

int ImageStream::getHeight() { return m_height; }
