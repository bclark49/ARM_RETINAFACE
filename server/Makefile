CXX = g++
CPPFLAGS = -pipe -O2 -std=gnu++11 -g -fopenmp -Wall -Wextra $(shell pkg-config --cflags ncnn opencv4 gstreamer-1.0 gstreamer-app-1.0 glib-2.0)
LINK = g++
LFLAGS = -O1
LDLIBS = $(shell pkg-config --libs ncnn opencv4 gstreamer-1.0 gstreamer-app-1.0 glib-2.0) -L/usr/lib/gcc/aarch64-linux-gnu -lgomp -lpthread -lm
TARGET = server

SRCDIR = src
BUILDDIR = build
SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

$(TARGET): $(OBJS) $(SRCDIR)/*.h
	$(LINK) $(LFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) -c $(CPPFLAGS) -I$(SRCDIR) -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(TARGET) $(BUILDDIR)

.PHONY: clean
