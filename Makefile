CXX = g++
CPPFLAGS = $(shell pkg-config --cflags opencv4 ncnn)
LDLIBS = $(shell pkg-config --libs opencv4 ncnn) -L/usr/lib/gcc/aarch64-linux-gnu -lgomp -lpthread -lm

% : %.cpp
	$(CXX) $< -g -o $@ $(CPPFLAGS) $(LDLIBS)
