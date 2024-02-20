CXX := g++
CPPFLAGS := -g -Wall $(shell pkg-config --cflags ncnn opencv4 gstreamer-1.0 gstreamer-app-1.0 glib-2.0) -I/usr/include/X11
LDLIBS := $(shell pkg-config --libs ncnn opencv4 gstreamer-1.0 gstreamer-app-1.0 glib-2.0) -L/usr/lib/gcc/aarch64-linux-gnu -lgomp -lpthread -lm -lX11
TARGET := server

SRCDIR := src
BUILDDIR := build
INCDIR := src
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

$(TARGET) : $(OBJS) $(INCDIR)/*.h
	$(CXX) $^ -o $@ $(CPPFLAGS) $(LDLIBS)

$(BUILDDIR)/%.o : $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CPPFLAGS) -I$(INCDIR) $(LDLIBS) -c $< -o $@ 

$(BUILDDIR) :
	mkdir -p $(BUILDDIR)

clean : 
	rm -rf $(TARGET) $(BUILDDIR) 

.PHONY : clean
