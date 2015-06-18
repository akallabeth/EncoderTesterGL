/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "glwidget.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QFileInfo>
#include <QFileInfoList>
#include <QOpenGLPaintDevice>
#include <QOpenGLFramebufferObject>

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      clearColor(Qt::black),
      program(0),
      currentTexture(0),
      currentFrame(0),
      numberOfFrames(0),
      currentFps(0),
      avgFrames(0),
      mFpsTimePerFrame(0)
{
    connect(&mFpsLimiter, &QTimer::timeout, this, &GLWidget::update);
    connect(&mTimer, &QTimer::timeout, this, &GLWidget::updateFps);
    mTimer.start(1000);
}

GLWidget::~GLWidget()
{
    makeCurrent();
    vbo.destroy();
    foreach(QOpenGLTexture *texture, textures)
	delete texture;
    delete program;
    doneCurrent();
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

void GLWidget::setClearColor(const QColor &color)
{
    clearColor = color;
    update();
}

void GLWidget::setFpsLimit(int limit)
{
    mFpsTimePerFrame = 1000 / limit;
}

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    makeObject();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

#define PROGRAM_VERTEX_ATTRIBUTE 0
#define PROGRAM_TEXCOORD_ATTRIBUTE 1

    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    const char *vsrc =
	    "attribute highp vec4 vertex;\n"
	    "attribute mediump vec4 texCoord;\n"
	    "varying mediump vec4 texc;\n"
	    "uniform mediump mat4 matrix;\n"
	    "void main(void)\n"
	    "{\n"
	    "    gl_Position = matrix * vertex;\n"
	    "    texc = texCoord;\n"
	    "    gl_Position = matrix * vertex;\n"
	    "    texc = texCoord * vec4(1.0, -1.0, 1.0, 1.0);\n"



	    "}\n";
    vshader->compileSourceCode(vsrc);

    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    const char *fsrc =
	    "uniform sampler2D texture;\n"
	    "varying mediump vec4 texc;\n"
	    "void main(void)\n"
	    "{\n"
	    "    gl_FragColor = texture2D(texture, texc.st);\n"
	    "    if (gl_FragColor.a < 0.5)\n"
	    "        discard;\n"
	    "}\n";
    fshader->compileSourceCode(fsrc);

    program = new QOpenGLShaderProgram;
    program->addShader(vshader);
    program->addShader(fshader);
    program->bindAttributeLocation("vertex", PROGRAM_VERTEX_ATTRIBUTE);
    program->bindAttributeLocation("texCoord", PROGRAM_TEXCOORD_ATTRIBUTE);
    program->link();

    program->bind();
    program->setUniformValue("texture", 0);
}

void GLWidget::paintGL()
{
    glClearColor(clearColor.redF(), clearColor.greenF(), clearColor.blueF(), clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    paintText();

    QMatrix4x4 m;
    m.ortho(-1.0f, +1.0f, +1.0f, -1.0f, 1.0f, -1.0f);

    program->setUniformValue("matrix", m);
    program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);
    program->enableAttributeArray(PROGRAM_TEXCOORD_ATTRIBUTE);
    program->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0,
				3, 5 * sizeof(GLfloat));
    program->setAttributeBuffer(PROGRAM_TEXCOORD_ATTRIBUTE, GL_FLOAT,
				3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

    if (textures.size()) {
	textures[currentTexture]->bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	currentTexture++;
	currentTexture %= textures.size();
    }

    currentFrame++;

    mFpsLimiter.start(mFpsTimePerFrame);
}
void GLWidget::resizeGL(int width, int height)
{
    qDebug() << width << "x" << height;
    glViewport(0, 0, width, height);
}

void GLWidget::paintText()
{
    QString fpsOverlay = QString("FPS: %1").arg(currentFps);
    QString avgOverlay = QString("AVG: %1").arg(avgFrames);

    const QRect drawRect(0, 0, 150, 50);
    const QSize drawRectSize = drawRect.size();

    QImage img(drawRectSize, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter painter;

    painter.begin(&img);
    painter.setBackgroundMode(Qt::TransparentMode);

    painter.setBackground(Qt::transparent);
    QFont font = painter.font();
    font.setPointSize(24);
    font.setPointSize(16);
    painter.setFont(font);
    painter.setPen(Qt::green);
    painter.setBrush(QBrush());

    QFontMetrics fMetrics = painter.fontMetrics();
    QSize fpsMetrics = fMetrics.size( Qt::TextSingleLine, fpsOverlay );
    QRectF fpsRect( QPoint(0, 0), fpsMetrics );
    QSize avgMetrics = fMetrics.size( Qt::TextSingleLine, avgOverlay );
    QRectF avgRect( QPoint(0, fpsRect.height()), avgMetrics );
    painter.drawText(fpsRect, Qt::TextSingleLine, fpsOverlay);
    painter.drawText(avgRect, Qt::TextSingleLine, avgOverlay);

    painter.end();

    QOpenGLTexture texture(img);
    texture.bind();

    QMatrix4x4 m;
    m.ortho(-12.0f, +12.0f, +12.0f, -12.0f, 1.0f, -1.0f);

    program->setUniformValue("matrix", m);
    program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);
    program->enableAttributeArray(PROGRAM_TEXCOORD_ATTRIBUTE);
    program->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0,
				3, 5 * sizeof(GLfloat));
    program->setAttributeBuffer(PROGRAM_TEXCOORD_ATTRIBUTE, GL_FLOAT,
				3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

}

void GLWidget::updateFps()
{
    currentFps = currentFrame;
    currentFrame = 0;

    avgFrames = (currentFps + numberOfFrames * avgFrames) / (numberOfFrames + 1);
    numberOfFrames++;

    qDebug() << "FPS: " << currentFps << ", AVG: " << avgFrames;
}

void GLWidget::makeObject()
{
    static const int coords[4][3] = {
	{ +1, -1, -1 },
	{ -1, -1, -1 },
	{ -1, +1, -1 },
	{ +1, +1, -1 }
    };

    QString path = QApplication::applicationDirPath();

    QDir dir(path, "*.png", QDir::IgnoreCase, QDir::Files);
    QFileInfoList files = dir.entryInfoList();

    foreach(QFileInfo file, files) {
	QImage img;
	if (img.load(file.absoluteFilePath())) {
	    textures.append(new QOpenGLTexture(img));
	}
    }

    QVector<GLfloat> vertData;
    for (int j = 0; j < 4; ++j) {
	// vertex position
	vertData.append(coords[j][0]);
	vertData.append(coords[j][1]);
	vertData.append(coords[j][2]);
	// texture coordinate
	vertData.append(j == 0 || j == 3);
	vertData.append(j == 0 || j == 1);
    }

    vbo.create();
    vbo.bind();
    vbo.allocate(vertData.constData(), vertData.count() * sizeof(GLfloat));
}

