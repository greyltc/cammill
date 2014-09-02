


INCLUDES = -I./
LIBS = -lGL -lglut -lGLU -lX11 -lm -lpthread -lstdc++ -lXext -ldl -lXi -lxcb -lXau -lXdmcp -lgcc -lc `pkg-config gtk+-2.0 --libs` `pkg-config gtk+-2.0 --cflags` `pkg-config gtkglext-x11-1.0 --libs` `pkg-config gtkglext-x11-1.0 --cflags`

#CFLAGS+="-DGTK_DISABLE_SINGLE_INCLUDES"
CFLAGS+="-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
CFLAGS+="-DGSEAL_ENABLE"



all: opencam2d

opencam2d: main.c setup.c dxf.c dxf.h font.c font.h texture.c
	clang -ggdb -Wall -O3 -o opencam2d main.c setup.c dxf.c font.c texture.c ${LIBS} ${INCLUDES} ${CFLAGS}

gprof:
	gcc -pg -Wall -O3 -o opencam2d main.c setup.c dxf.c font.c texture.c ${LIBS} ${INCLUDES} ${CFLAGS}
	echo "./opencam2d"
	echo "gprof opencam2d gmon.out"

clean:
	rm -rf opencam2d




