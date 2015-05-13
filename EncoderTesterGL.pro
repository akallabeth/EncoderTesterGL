HEADERS       = glwidget.h \
                window.h
SOURCES       = glwidget.cpp \
                main.cpp \
                window.cpp

RESOURCES     = EncoderTesterGL.qrc

QT           += widgets gui

# install
# target.path = $$[QT_INSTALL_EXAMPLES]/opengl/textures
INSTALLS += target
