/*

 Copyright 2014 by Oliver Dippel <oliver@multixmedia.org>

 MacOSX - Changes by McUles <mcules@fpv-club.de>
	Yosemite (OSX 10.10)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 2 of the License, or (at
 your option) any later version.

 On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in `/usr/share/common-licenses/GPL'.

*/

#include <GL/gl.h>
#include <GL/glu.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#include <libgen.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <pwd.h>
#include <dxf.h>
#include <font.h>
#include <setup.h>
#include <postprocessor.h>


void texture_init (void);
GLuint texture_load (char *filename);

#define FUZZY 0.001

#ifndef CALLBACK
#define CALLBACK
#endif

int winw = 1600;
int winh = 1200;
float size_x = 0.0;
float size_y = 0.0;
double min_x = 99999.0;
double min_y = 99999.0;
double max_x = 0.0;
double max_y = 0.0;
int mill_start = 0;
int mill_start_all = 0;
double mill_last_x = 0.0;
double mill_last_y = 0.0;
double mill_last_z = 0.0;
double tooltbl_diameters[100];
char mill_layer[1024];
int layer_sel = 0;
int layer_selections[100];
int layer_force[100];
double layer_depth[100];
FILE *fd_out = NULL;
int object_last = 0;
int batchmode = 0;
int save_gcode = 0;
char tool_descr[100][1024];
int tools_max = 0;
int tool_last = 0;
int LayerMax = 1;
int material_max = 8;
char *material_texture[100];
int material_vc[100];
float material_fz[100][3];
char *rotary_axis[3] = {"A", "B", "C"};
char cline[1024];
char postcam_plugins[100][1024];
int postcam_plugin = -1;
int update_post = 1;
char *gcode_buffer = NULL;
char output_extension[128];
char output_info[1024];
char output_error[1024];
int loading = 0;

int last_mouse_x = 0;
int last_mouse_y = 0;
int last_mouse_button = -1;
int last_mouse_state = 0;

void ParameterUpdate (void);
void ParameterChanged (GtkWidget *widget, gpointer data);

GtkWidget *gCodeViewLabel;
GtkWidget *gCodeViewLabelLua;
GtkWidget *OutputInfoLabel;
GtkWidget *OutputErrorLabel;
GtkWidget *SizeInfoLabel;
GtkWidget *StatusBar;
GtkTreeStore *treestore;
GtkListStore *ListStore[P_LAST];
GtkWidget *ParamValue[P_LAST];
GtkWidget *ParamButton[P_LAST];
GtkWidget *glCanvas;
GtkWidget *gCodeView;
GtkWidget *gCodeViewLua;
int width = 800;
int height = 600;
int need_init = 1;

double mill_distance_xy = 0.0;
double mill_distance_z = 0.0;
double move_distance_xy = 0.0;
double move_distance_z = 0.0;

GtkWidget *window;
GtkWidget *dialog;



void append_gcode (char *text) {
#ifdef USE_POSTCAM
	return;
#endif
	if (gcode_buffer == NULL) {
		int len = strlen(text) + 1;
		gcode_buffer = malloc(len);
		gcode_buffer[0] = 0;
	} else {
		int len = strlen(gcode_buffer) + strlen(text) + 1;
		gcode_buffer = realloc(gcode_buffer, len);
	}
	strcat(gcode_buffer, text);
}

void append_gcode_new (char *text) {
	if (gcode_buffer == NULL) {
		int len = strlen(text) + 1;
		gcode_buffer = malloc(len);
		gcode_buffer[0] = 0;
	} else {
		int len = strlen(gcode_buffer) + strlen(text) + 1;
		gcode_buffer = realloc(gcode_buffer, len);
	}
	strcat(gcode_buffer, text);
}

void line_invert (int num) {
	double tempx = myLINES[num].x2;
	double tempy = myLINES[num].y2;
	myLINES[num].x2 = myLINES[num].x1;
	myLINES[num].y2 = myLINES[num].y1;
	myLINES[num].x1 = tempx;
	myLINES[num].y1 = tempy;
	myLINES[num].opt *= -1;
}


int point_in_object (int object_num, int object_ex, double testx, double testy) {
	int num = 0;
	int c = 0;
	int onum = object_num;
	if (object_num == -1) {
		for (onum = 0; onum < object_last; onum++) {
			if (onum == object_ex) {
				continue;
			}
			if (myOBJECTS[onum].closed == 0) {
				continue;
			}
			for (num = 0; num < line_last; num++) {
				if (myOBJECTS[onum].line[num] != 0) {
					int lnum = myOBJECTS[onum].line[num];
					if (((myLINES[lnum].y2 >= testy) != (myLINES[lnum].y1 >= testy)) && (testx <= (myLINES[lnum].x1 - myLINES[lnum].x2) * (testy - myLINES[lnum].y2) / (myLINES[lnum].y1 - myLINES[lnum].y2) + myLINES[lnum].x2)) {
						c = !c;
					}
				}
			}
		}
	} else {
		if (myOBJECTS[onum].closed == 0) {
			return 0;
		}
		for (num = 0; num < line_last; num++) {
			if (myOBJECTS[onum].line[num] != 0) {
				int lnum = myOBJECTS[onum].line[num];
				if (((myLINES[lnum].y2 >= testy) != (myLINES[lnum].y1 >= testy)) && (testx <= (myLINES[lnum].x1 - myLINES[lnum].x2) * (testy - myLINES[lnum].y2) / (myLINES[lnum].y1 - myLINES[lnum].y2) + myLINES[lnum].x2)) {
					c = !c;
				}
			}
		}
	}
	return c;
}

void CALLBACK beginCallback(GLenum which) {
   glBegin(which);
}

void CALLBACK errorCallback(GLenum errorCode) {
	const GLubyte *estring;
	estring = gluErrorString(errorCode);
//	fprintf(stderr, "Tessellation Error: %s\n", (char *) estring);
//	exit(0);
}

void CALLBACK endCallback(void) {
	glEnd();
}

void CALLBACK vertexCallback(GLvoid *vertex) {
	const GLdouble *pointer;
	pointer = (GLdouble *) vertex;
	glColor3dv(pointer+3);
	glVertex3dv(pointer);
}

void CALLBACK combineCallback(GLdouble coords[3], GLdouble *vertex_data[4], GLfloat weight[4], GLdouble **dataOut ) {
	GLdouble *vertex;
	int i;
	vertex = (GLdouble *)malloc(6 * sizeof(GLdouble));
	vertex[0] = coords[0];
	vertex[1] = coords[1];
	vertex[2] = coords[2];
	for (i = 3; i < 6; i++) {
		vertex[i] = weight[0] * vertex_data[0][i] + weight[1] * vertex_data[1][i] + weight[2] * vertex_data[2][i] + weight[3] * vertex_data[3][i];
	}
	*dataOut = vertex;
}

void point_rotate (float y, float depth, float *ny, float *nz) {
	float radius = (PARAMETER[P_MAT_DIAMETER].vdouble / 2.0) + depth;
	float an = y / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
	float rangle = toRad(an - 90.0);
	*ny = radius * cos(rangle);
	*nz = radius * -sin(rangle);
}

double _X (double x) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1 && PARAMETER[P_H_ROTARYAXIS].vint == 1) {
		return x / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
	}
	return x;
}

double _Y (double y) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1 && PARAMETER[P_H_ROTARYAXIS].vint == 0) {
		return y / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
	}
	return y;
}

double _Z (double z) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		return z + (PARAMETER[P_MAT_DIAMETER].vdouble / 2.0);
	}
	return z;
}

void translateAxisX (double x, char *ret_str) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1 && PARAMETER[P_H_ROTARYAXIS].vint == 1) {
		double an = x / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
		sprintf(ret_str, "%s%f", rotary_axis[PARAMETER[P_H_ROTARYAXIS].vint], an);
	} else {
		sprintf(ret_str, "X%f", x);
	}
}

void translateAxisY (double y, char *ret_str) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1 && PARAMETER[P_H_ROTARYAXIS].vint == 0) {
		double an = y / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
		sprintf(ret_str, "%s%f", rotary_axis[PARAMETER[P_H_ROTARYAXIS].vint], an);
	} else {
		sprintf(ret_str, "Y%f", y);
	}
}

void translateAxisZ (double z, char *ret_str) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		sprintf(ret_str, "Z%f", z + (PARAMETER[P_MAT_DIAMETER].vdouble / 2.0));
	} else {
		sprintf(ret_str, "Z%f", z);
	}
}

void object2poly (int object_num, double depth, double depth2, int invert) {
	int num = 0;
	int nverts = 0;
	GLUtesselator *tobj;
	GLdouble rect2[MAX_LINES][3];
	if (PARAMETER[P_V_TEXTURES].vint == 1) {
		glColor4f(1.0, 1.0, 1.0, 1.0);
		texture_load(material_texture[PARAMETER[P_MAT_SELECT].vint]);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glTexGend(GL_S,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);
		glTexGend(GL_T,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef(0.002, 0.002, 0.002);
	} else  {
		if (invert == 0) {
			glColor4f(0.0, 0.5, 0.2, 0.5);
		} else {
			glColor4f(0.0, 0.75, 0.3, 0.5);
		}
	}
	tobj = gluNewTess();
	gluTessCallback(tobj, GLU_TESS_VERTEX, (GLvoid (CALLBACK*) ()) &glVertex3dv);
	gluTessCallback(tobj, GLU_TESS_BEGIN, (GLvoid (CALLBACK*) ()) &beginCallback);
	gluTessCallback(tobj, GLU_TESS_END, (GLvoid (CALLBACK*) ()) &endCallback);
	gluTessCallback(tobj, GLU_TESS_ERROR, (GLvoid (CALLBACK*) ()) &errorCallback);
	glShadeModel(GL_FLAT);
	gluTessBeginPolygon(tobj, NULL);
	if (invert == 0) {
		if (myOBJECTS[object_num].climb == 0) {
			gluTessProperty(tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_POSITIVE);
		} else {
			gluTessProperty(tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NEGATIVE);
		}
	} else {
		if (myOBJECTS[object_num].climb == 0) {
			gluTessProperty(tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NEGATIVE);
		} else {
			gluTessProperty(tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_POSITIVE);
		}
	}
	gluTessNormal(tobj, 0, 0, 1);
	gluTessBeginContour(tobj);
	if (myLINES[myOBJECTS[object_num].line[0]].type == TYPE_CIRCLE) {
		gluTessProperty(tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_POSITIVE);
		int lnum = myOBJECTS[object_num].line[0];
		float an = 0.0;
		float r = myLINES[lnum].opt;
		float x = myLINES[lnum].cx;
		float y = myLINES[lnum].cy;
		float last_x = x + r;
		float last_y = y;
		for (an = 0.0; an < 360.0; an += 9.0) {
			float angle1 = toRad(an);
			float x1 = r * cos(angle1);
			float y1 = r * sin(angle1);
			rect2[nverts][0] = (GLdouble)x + x1;
			rect2[nverts][1] = (GLdouble)y + y1;
			rect2[nverts][2] = (GLdouble)depth;
			gluTessVertex(tobj, rect2[nverts], rect2[nverts]);
			if (depth != depth2) {
				glBegin(GL_QUADS);
				glVertex3f((float)last_x, (float)last_y, depth);
				glVertex3f((float)x + x1, (float)y + y1, depth);
				glVertex3f((float)x + x1, (float)y + y1, depth2);
				glVertex3f((float)last_x, (float)last_y, depth2);
				glEnd();
			}
			last_x = (float)x + x1;
			last_y = (float)y + y1;
			nverts++;
		}
	} else {
		for (num = 0; num < line_last; num++) {
			if (myOBJECTS[object_num].line[num] != 0) {
				int lnum = myOBJECTS[object_num].line[num];
				rect2[nverts][0] = (GLdouble)myLINES[lnum].x1;
				rect2[nverts][1] = (GLdouble)myLINES[lnum].y1;
				rect2[nverts][2] = (GLdouble)depth;
				gluTessVertex(tobj, rect2[nverts], rect2[nverts]);
				if (depth != depth2) {
					glBegin(GL_QUADS);
					glVertex3f((float)myLINES[lnum].x1, (float)myLINES[lnum].y1, depth);
					glVertex3f((float)myLINES[lnum].x2, (float)myLINES[lnum].y2, depth);
					glVertex3f((float)myLINES[lnum].x2, (float)myLINES[lnum].y2, depth2);
					glVertex3f((float)myLINES[lnum].x1, (float)myLINES[lnum].y1, depth2);
					glEnd();
				}
				nverts++;
			}
		}
	}
	int num5 = 0;
	for (num5 = 0; num5 < object_last; num5++) {
		if (num5 != object_num && myOBJECTS[num5].closed == 1 && myOBJECTS[num5].inside == 1) {
			int lnum = myOBJECTS[num5].line[0];
			int pipret = 0;
			double testx = myLINES[lnum].x1;
			double testy = myLINES[lnum].y1;
			pipret = point_in_object(object_num, -1, testx, testy);
			if (pipret == 1) {
				gluNextContour(tobj, GLU_INTERIOR);

				if (myLINES[lnum].type == TYPE_CIRCLE) {
					float an = 0.0;
					float r = myLINES[lnum].opt;
					float x = myLINES[lnum].cx;
					float y = myLINES[lnum].cy;
					float last_x = x + r;
					float last_y = y;
					if (myOBJECTS[object_num].climb == 1) {
						for (an = 0.0; an < 360.0; an += 9.0) {
							float angle1 = toRad(an);
							float x1 = r * cos(angle1);
							float y1 = r * sin(angle1);
							rect2[nverts][0] = (GLdouble)x + x1;
							rect2[nverts][1] = (GLdouble)y + y1;
							rect2[nverts][2] = (GLdouble)depth;
							gluTessVertex(tobj, rect2[nverts], rect2[nverts]);
							if (depth != depth2) {
								glBegin(GL_QUADS);
								glVertex3f((float)last_x, (float)last_y, depth);
								glVertex3f((float)x + x1, (float)y + y1, depth);
								glVertex3f((float)x + x1, (float)y + y1, depth2);
								glVertex3f((float)last_x, (float)last_y, depth2);
								glEnd();
							}
							last_x = (float)x + x1;
							last_y = (float)y + y1;
							nverts++;
						}
					} else {
						for (an = 360.0; an > 0.0; an -= 9.0) {
							float angle1 = toRad(an);
							float x1 = r * cos(angle1);
							float y1 = r * sin(angle1);
							rect2[nverts][0] = (GLdouble)x + x1;
							rect2[nverts][1] = (GLdouble)y + y1;
							rect2[nverts][2] = (GLdouble)depth;
							gluTessVertex(tobj, rect2[nverts], rect2[nverts]);
							if (depth != depth2) {
								glBegin(GL_QUADS);
								glVertex3f((float)last_x, (float)last_y, depth);
								glVertex3f((float)x + x1, (float)y + y1, depth);
								glVertex3f((float)x + x1, (float)y + y1, depth2);
								glVertex3f((float)last_x, (float)last_y, depth2);
								glEnd();
							}
							last_x = (float)x + x1;
							last_y = (float)y + y1;
							nverts++;
						}
					}
				} else {
					for (num = 0; num < line_last; num++) {
						if (myOBJECTS[num5].line[num] != 0) {
							int lnum = myOBJECTS[num5].line[num];
							rect2[nverts][0] = (GLdouble)myLINES[lnum].x1;
							rect2[nverts][1] = (GLdouble)myLINES[lnum].y1;
							rect2[nverts][2] = (GLdouble)depth;
							gluTessVertex(tobj, rect2[nverts], rect2[nverts]);
							if (depth != depth2) {
								glBegin(GL_QUADS);
								glVertex3f((float)myLINES[lnum].x1, (float)myLINES[lnum].y1, depth);
								glVertex3f((float)myLINES[lnum].x2, (float)myLINES[lnum].y2, depth);
								glVertex3f((float)myLINES[lnum].x2, (float)myLINES[lnum].y2, depth2);
								glVertex3f((float)myLINES[lnum].x1, (float)myLINES[lnum].y1, depth2);
								glEnd();
							}
							nverts++;
						}
					}
				}
			}
		}
	}
	gluTessEndPolygon(tobj);
	gluTessCallback(tobj, GLU_TESS_VERTEX, (GLvoid (CALLBACK*) ()) &vertexCallback);
	gluTessCallback(tobj, GLU_TESS_BEGIN, (GLvoid (CALLBACK*) ()) &beginCallback);
	gluTessCallback(tobj, GLU_TESS_END, (GLvoid (CALLBACK*) ()) &endCallback);
	gluTessCallback(tobj, GLU_TESS_ERROR, (GLvoid (CALLBACK*) ()) &errorCallback);
	gluTessCallback(tobj, GLU_TESS_COMBINE, (GLvoid (CALLBACK*) ()) &combineCallback);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_TEXTURE_2D);
	glMatrixMode(GL_MODELVIEW);
	gluDeleteTess(tobj);
}

int object_line_last (int object_num) {
	int num = 0;
	int ret = 0;
	for (num = 0; num < line_last; num++) {
		if (myOBJECTS[object_num].line[num] != 0) {
			ret = num;
		}
	}
	return ret;
}

double get_len (double x1, double y1, double x2, double y2) {
	double dx = x2 - x1;
	double dy = y2 - y1;
	double len = sqrt(dx * dx + dy * dy);
	return len;
}

double set_positive (double val) {
	if (val < 0.0) {
		val *= -1;
	}
	return val;
}

/* set new first line in object */
void resort_object (int object_num, int start) {
	int num4 = 0;
	int num5 = 0;
	int OTEMPLINE[MAX_LINES];
	for (num4 = 0; num4 < line_last; num4++) {
		OTEMPLINE[num4] = 0;
	}
	for (num4 = start; num4 < line_last; num4++) {
		if (myOBJECTS[object_num].line[num4] != 0) {
			OTEMPLINE[num5++] = myOBJECTS[object_num].line[num4];
		}
	}
	for (num4 = 0; num4 < start; num4++) {
		if (myOBJECTS[object_num].line[num4] != 0) {
			OTEMPLINE[num5++] = myOBJECTS[object_num].line[num4];
		}
	}
	for (num4 = 0; num4 < num5; num4++) {
		myOBJECTS[object_num].line[num4] = OTEMPLINE[num4];
	}
}

/* reverse lines in object */
void redir_object (int object_num) {
	int num = 0;
	int num5 = 0;
	int OTEMPLINE[MAX_LINES];
	for (num = 0; num < line_last; num++) {
		OTEMPLINE[num] = 0;
	}
	for (num = line_last - 1; num >= 0; num--) {
		if (myOBJECTS[object_num].line[num] != 0) {
			OTEMPLINE[num5++] = myOBJECTS[object_num].line[num];
			int lnum = myOBJECTS[object_num].line[num];
			line_invert(lnum);
		}
	}
	for (num = 0; num < num5; num++) {
		myOBJECTS[object_num].line[num] = OTEMPLINE[num];
	}
}

double line_len (int lnum) {
	double dx = myLINES[lnum].x2 - myLINES[lnum].x1;
	double dy = myLINES[lnum].y2 - myLINES[lnum].y1;
	double len = sqrt(dx * dx + dy * dy);
	return len;
}

double line_angle (int lnum) {
	double dx = myLINES[lnum].x2 - myLINES[lnum].x1;
	double dy = myLINES[lnum].y2 - myLINES[lnum].y1;
	double alpha = toDeg(atan(dy / dx));
	if (dx < 0 && dy >= 0) {
		alpha = alpha + 180;
	} else if (dx < 0 && dy < 0) {
		alpha = alpha - 180;
	}
	return alpha;
}

double vector_angle (double x1, double y1, double x2, double y2) {
	double dx = x2 - x1;
	double dy = y2 - y1;
	double alpha = toDeg(atan(dy / dx));
	if (dx < 0 && dy >= 0) {
		alpha = alpha + 180;
	} else if (dx < 0 && dy < 0) {
		alpha = alpha - 180;
	}
	return alpha;
}

double line_angle2 (int lnum) {
	double dx = myLINES[lnum].x2 - myLINES[lnum].x1;
	double dy = myLINES[lnum].y2 - myLINES[lnum].y1;
	double alpha = toDeg(atan2(dx, dy));
	return alpha;
}

void add_angle_offset (double *check_x, double *check_y, double radius, double alpha) {
	double angle = toRad(alpha);
	*check_x += radius * cos(angle);
	*check_y += radius * sin(angle);
}

/* optimize dir of object / inside=cw, outside=ccw */
void object_optimize_dir (int object_num) {
	int pipret = 0;
	if (myOBJECTS[object_num].line[0] != 0) {
		if (myOBJECTS[object_num].closed == 1) {
			int lnum = myOBJECTS[object_num].line[0];
			double alpha = line_angle(lnum);
			double len = line_len(lnum);
			double check_x = myLINES[lnum].x1;
			double check_y = myLINES[lnum].y1;
			add_angle_offset(&check_x, &check_y, len / 2.0, alpha);
			if (myOBJECTS[object_num].climb == 0) {
				add_angle_offset(&check_x, &check_y, -0.01, alpha + 90);
			} else {
				add_angle_offset(&check_x, &check_y, 0.01, alpha + 90);
			}
			pipret = point_in_object(object_num, -1, check_x, check_y);
			if (myOBJECTS[object_num].force == 1) {
				if ((pipret == 1 && myOBJECTS[object_num].offset == 2) || (pipret == 0 && myOBJECTS[object_num].offset == 1)) {
					redir_object(object_num);
				}
			} else {
				if ((pipret == 1 && myOBJECTS[object_num].inside == 0) || (pipret == 0 && myOBJECTS[object_num].inside == 1)) {
					redir_object(object_num);
				}
			}
		}
	}
}

int intersect_check (double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y, double p3_x, double p3_y, double *i_x, double *i_y) {
	double s02_x, s02_y, s10_x, s10_y, s32_x, s32_y, s_numer, t_numer, denom, t;
	s10_x = p1_x - p0_x;
	s10_y = p1_y - p0_y;
	s02_x = p0_x - p2_x;
	s02_y = p0_y - p2_y;
	s_numer = s10_x * s02_y - s10_y * s02_x;
	if (s_numer < 0) {
		return 0;
	}
	s32_x = p3_x - p2_x;
	s32_y = p3_y - p2_y;
	t_numer = s32_x * s02_y - s32_y * s02_x;
	if (t_numer < 0) {
		return 0;
	}
	denom = s10_x * s32_y - s32_x * s10_y;
	if (s_numer > denom || t_numer > denom) {
		return 0;
	}
	t = t_numer / denom;
	if (i_x != NULL) {
		*i_x = p0_x + (t * s10_x);
	}
	if (i_y != NULL) {
		*i_y = p0_y + (t * s10_y);
	}
	return 1;
}

void intersect (double l1x1, double l1y1, double l1x2, double l1y2, double l2x1, double l2y1, double l2x2, double l2y2, double *x, double *y) {
	double a1 = l1x2 - l1x1;
	double b1 = l2x1 - l2x2;
	double c1 = l2x1 - l1x1;
	double a2 = l1y2 - l1y1;
	double b2 = l2y1 - l2y2;
	double c2 = l2y1 - l1y1;
	double t = (b1 * c2 - b2 * c1) / (a2 * b1 - a1 * b2);
	*x = l1x1 + t * a1;
	*y = l1y1 + t * a2;
	return;
}

void postcam_load_source (char *plugin) {
	char tmp_str[1024];
	sprintf(tmp_str, "posts/%s.scpost", plugin);
	GtkTextBuffer *bufferLua = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCodeViewLua));
	gchar *file_buffer;
	GError *error;
	gboolean read_file_status = g_file_get_contents(tmp_str, &file_buffer, NULL, &error);
	if (read_file_status == FALSE) {
		g_error("error opening file: %s\n",error && error->message ? error->message : "No Detail");
		return;
	}

	gtk_text_buffer_set_text(bufferLua, file_buffer, -1);
	free(file_buffer);
}

void postcam_save_source (char *plugin) {
	char tmp_str[1024];
	sprintf(tmp_str, "posts/%s.scpost", plugin);
	FILE *fp = fopen(tmp_str, "w");
	if (fp != NULL) {
		GtkTextIter start, end;
		GtkTextBuffer *bufferLua;
		bufferLua = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCodeViewLua));
		gtk_text_buffer_get_bounds(bufferLua, &start, &end);
		char *luacode = gtk_text_buffer_get_text(bufferLua, &start, &end, TRUE);
		fprintf(fp, "%s", luacode);
		fclose(fp);
		free(luacode);
	}
}

void DrawLine (float x1, float y1, float x2, float y2, float z, float w) {
	float angle = atan2(y2 - y1, x2 - x1);
	float t2sina1 = w / 2 * sin(angle);
	float t2cosa1 = w / 2 * cos(angle);
	glBegin(GL_QUADS);
	glVertex3f(x1 + t2sina1, y1 - t2cosa1, z);
	glVertex3f(x2 + t2sina1, y2 - t2cosa1, z);
	glVertex3f(x2 - t2sina1, y2 + t2cosa1, z);
	glVertex3f(x1 - t2sina1, y1 + t2cosa1, z);
	glEnd();
}

void DrawArrow (float x1, float y1, float x2, float y2, float z, float w) {
	float dx = x2 - x1;
	float dy = y2 - y1;
	float len = sqrt(dx * dx + dy * dy);
	float asize = 2.0;
	if (len < asize) {
		return;
	}
	float angle = atan2(dy, dx);
	float off_x = asize * cos(angle + toRad(45.0 + 90.0));
	float off_y = asize * sin(angle + toRad(45.0 + 90.0));
	float off2_x = asize * cos(angle + toRad(-45.0 - 90.0));
	float off2_y = asize * sin(angle + toRad(-45.0 - 90.0));
	float half_x = x1 + (x2 - x1) / 2.0;
	float half_y = y1 + (y2 - y1) / 2.0;
	glBegin(GL_LINES);
	glVertex3f(half_x, half_y, z);
	glVertex3f(half_x + off_x, half_y + off_y, z);
	glVertex3f(half_x, half_y, z);
	glVertex3f(half_x + off2_x, half_y + off2_y, z);
	glEnd();
}

void draw_line_wrap_conn (float x1, float y1, float depth1, float depth2) {
	float ry = 0.0;
	float rz = 0.0;
	glBegin(GL_LINES);
	point_rotate(y1, depth1, &ry, &rz);
	glVertex3f(x1, ry, rz);
	point_rotate(y1, depth2, &ry, &rz);
	glVertex3f(x1, ry, rz);
	glEnd();
}

void draw_line_wrap (float x1, float y1, float x2, float y2, float depth) {
	float radius = (PARAMETER[P_MAT_DIAMETER].vdouble / 2.0) + depth;
	float dX = x2 - x1;
	float dY = y2 - y1;
	float dashes = dY;
	if (dashes < -1.0) {
		dashes *= -1;
	}
	float an = y1 / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
	float rangle = toRad(an - 90.0);
	float ry = radius * cos(rangle);
	float rz = radius * sin(rangle);
	glBegin(GL_LINE_STRIP);
	glVertex3f(x1, ry, -rz);
	if (dashes > 1.0) {
		float dashX = dX / dashes;
		float dashY = dY / dashes;
		float q = 0.0;
		while (q++ < dashes) {
			x1 += dashX;
			y1 += dashY;
			an = y1 / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
			rangle = toRad(an - 90.0);
			ry = radius * cos(rangle);
			rz = radius * sin(rangle);
			glVertex3f(x1, ry, -rz);
		}
	}
	an = y2 / (PARAMETER[P_MAT_DIAMETER].vdouble * PI) * 360;
	rangle = toRad(an - 90.0);
	ry = radius * cos(rangle);
	rz = radius * sin(rangle);
	glVertex3f(x2, ry, -rz);
	glEnd();
}

void draw_oline (float x1, float y1, float x2, float y2, float depth) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		draw_line_wrap(x1, y1, x2, y2, 0.0);
		draw_line_wrap(x1, y1, x2, y2, depth);
		draw_line_wrap_conn(x1, y1, 0.0, depth);
		draw_line_wrap_conn(x2, y2, 0.0, depth);
	} else {
		glBegin(GL_LINES);
		glVertex3f(x1, y1, 0.02);
		glVertex3f(x2, y2, 0.02);
	//	if (PARAMETER[P_V_HELPLINES].vint == 1) {
			glBegin(GL_LINES);
			glVertex3f(x1, y1, depth);
			glVertex3f(x2, y2, depth);
			glVertex3f(x1, y1, depth);
			glVertex3f(x1, y1, 0.02);
			glVertex3f(x2, y2, depth);
			glVertex3f(x2, y2, 0.02);
	//	}
		glEnd();
	}
}

void draw_line2 (float x1, float y1, float z1, float x2, float y2, float z2, float width) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		draw_line_wrap(x1, y1, x2, y2, 0.0);
		draw_line_wrap(x1, y1, x2, y2, z1);
		draw_line_wrap_conn(x1, y1, 0.0, z1);
		draw_line_wrap_conn(x2, y2, 0.0, z1);
	} else {
		if (PARAMETER[P_V_HELPLINES].vint == 1) {
			DrawLine(x1, y1, x2, y2, z1, width);
			GLUquadricObj *quadric=gluNewQuadric();
			gluQuadricNormals(quadric, GLU_SMOOTH);
			gluQuadricOrientation(quadric,GLU_OUTSIDE);
			glPushMatrix();
			glTranslatef(x1, y1, z1);
			gluDisk(quadric, 0.0, width / 2.0, 18, 1);
			glPopMatrix();
			glPushMatrix();
			glTranslatef(x2, y2, z1);
			gluDisk(quadric, 0.0, width / 2.0, 18, 1);
			glPopMatrix();
			gluDeleteQuadric(quadric);
		}
		glBegin(GL_LINES);
		glVertex3f(x1, y1, z1);
		glVertex3f(x2, y2, z2);
		glEnd();
	}
}

void draw_line (float x1, float y1, float z1, float x2, float y2, float z2, float width) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		glColor4f(1.0, 0.0, 1.0, 1.0);
		draw_line_wrap(x1, y1, x2, y2, 0.0);
		draw_line_wrap(x1, y1, x2, y2, z1);
		draw_line_wrap_conn(x1, y1, 0.0, z1);
		draw_line_wrap_conn(x2, y2, 0.0, z1);
	} else {
		if (PARAMETER[P_V_HELPDIA].vint == 1) {
			glColor4f(1.0, 1.0, 0.0, 1.0);
			DrawLine(x1, y1, x2, y2, z1, width);
			GLUquadricObj *quadric=gluNewQuadric();
			gluQuadricNormals(quadric, GLU_SMOOTH);
			gluQuadricOrientation(quadric,GLU_OUTSIDE);
			glPushMatrix();
			glTranslatef(x1, y1, z1);
			gluDisk(quadric, 0.0, width / 2.0, 18, 1);
			glPopMatrix();
			glPushMatrix();
			glTranslatef(x2, y2, z1);
			gluDisk(quadric, 0.0, width / 2.0, 18, 1);
			glPopMatrix();
			gluDeleteQuadric(quadric);
		}
		glColor4f(1.0, 0.0, 1.0, 1.0);
		glBegin(GL_LINES);
		glVertex3f(x1, y1, z1 + 0.02);
		glVertex3f(x2, y2, z2 + 0.02);
		glEnd();
		DrawArrow(x1, y1, x2, y2, z1 + 0.02, width);
	}
}

void draw_line3 (float x1, float y1, float z1, float x2, float y2, float z2) {
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		glColor4f(1.0, 0.0, 1.0, 1.0);
		draw_line_wrap(x1, y1, x2, y2, z1);
	} else {
		glColor4f(1.0, 0.0, 1.0, 1.0);
		glBegin(GL_LINES);
		glVertex3f(x1, y1, z1 + 0.02);
		glVertex3f(x2, y2, z2 + 0.02);
		glEnd();
	}
}

void mill_z (int gcmd, double z) {
#ifdef USE_POSTCAM
	postcam_var_push_int("feedRate", PARAMETER[P_M_PLUNGE_SPEED].vint);
	postcam_var_push_double("currentX", _X(mill_last_x));
	postcam_var_push_double("currentY", _Y(mill_last_y));
	postcam_var_push_double("currentZ", _Z(mill_last_z));
	postcam_var_push_double("endX", _X(mill_last_x));
	postcam_var_push_double("endY", _Y(mill_last_y));
	postcam_var_push_double("endZ", _Z(z));
	if (gcmd == 0) {
		move_distance_z += set_positive(z - mill_last_z);
		postcam_call_function("OnRapid");
	} else {
		mill_distance_z += set_positive(z - mill_last_z);
		postcam_call_function("OnMove");
	}
#else
	char tz_str[128];
	translateAxisZ(z, tz_str);
	if (gcmd == 0) {
		sprintf(cline, "G0%i %s\n", gcmd, tz_str);
		append_gcode(cline);
	} else {
		sprintf(cline, "G0%i %s F%i\n", gcmd, tz_str, PARAMETER[P_M_PLUNGE_SPEED].vint);
		append_gcode(cline);
	}
	if (gcmd == 0) {
		move_distance_z += set_positive(z - mill_last_z);
	} else {
		mill_distance_z += set_positive(z - mill_last_z);
	}
#endif
	if (mill_start_all != 0) {
		glColor4f(0.0, 1.0, 1.0, 1.0);
		if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
			draw_line_wrap_conn((float)mill_last_x, (float)mill_last_y, (float)mill_last_z, (float)z);
		} else {
			glBegin(GL_LINES);
			glVertex3f((float)mill_last_x, (float)mill_last_y, (float)mill_last_z);
			glVertex3f((float)mill_last_x, (float)mill_last_y, (float)z);
			glEnd();
		}
	}
	mill_last_z = z;
}

void mill_xy (int gcmd, double x, double y, double r, int feed, char *comment) {
#ifndef USE_POSTCAM
	char tx_str[128];
	char ty_str[128];
	char tz_str[128];
#endif
	if (comment[0] != 0) {
		sprintf(cline, "(%s)\n", comment);
		append_gcode(cline);
	}
	if (gcmd != 0) {
		int hflag = 0;
		postcam_var_push_int("feedRate", feed);
		postcam_var_push_double("currentZ", _Z(mill_last_z));
		postcam_var_push_double("currentX", _X(mill_last_x));
		postcam_var_push_double("currentY", _Y(mill_last_y));
		postcam_var_push_double("endZ", _Z(mill_last_z));
		if (gcmd == 1) {
			double i_x = 0.0;
			double i_y = 0.0;
			int num = 0;
			int numr = 0;
			if (PARAMETER[P_T_USE].vint == 1) {
				if (PARAMETER[P_T_XGRID].vdouble == 0.0 && PARAMETER[P_T_YGRID].vdouble == 0.0) {
					int line_flag[MAX_LINES];
					for (num = 0; num < line_last; num++) {
						line_flag[num] = 0;
					}
					for (numr = 0; numr < line_last; numr++) {
						int min_dist_line = -1;
						double min_dist = 999999.0;
						for (num = 0; num < line_last; num++) {
							if (myLINES[num].istab == 1 && line_flag[num] == 0) {
								if (mill_last_z < PARAMETER[P_T_DEPTH].vdouble && (intersect_check(mill_last_x, mill_last_y, x, y, myLINES[num].x1 + 0.0002, myLINES[num].y1 + 0.0002, myLINES[num].x2 + 0.0002, myLINES[num].y2 + 0.0002, &i_x, &i_y) == 1 || intersect_check(x, y, mill_last_x, mill_last_y, myLINES[num].x1 + 0.0002, myLINES[num].y1 + 0.0002, myLINES[num].x2 + 0.0002, myLINES[num].y2 + 0.0002, &i_x, &i_y) == 1)) {
									double dist = set_positive(get_len(mill_last_x, mill_last_y, i_x, i_y));
									if (min_dist > dist) {
										min_dist = dist;
										min_dist_line = num;
									}
								} else {
									line_flag[num] = 1;
								}
							}
						}
						if (min_dist_line != -1) {
							line_flag[min_dist_line] = 1;
							num = min_dist_line;
							if (mill_last_z < PARAMETER[P_T_DEPTH].vdouble && (intersect_check(mill_last_x, mill_last_y, x, y, myLINES[num].x1 + 0.0002, myLINES[num].y1 + 0.0002, myLINES[num].x2 + 0.0002, myLINES[num].y2 + 0.0002, &i_x, &i_y) == 1 || intersect_check(x, y, mill_last_x, mill_last_y, myLINES[num].x1 + 0.0002, myLINES[num].y1 + 0.0002, myLINES[num].x2 + 0.0002, myLINES[num].y2 + 0.0002, &i_x, &i_y) == 1)) {
								double alpha1 = vector_angle(mill_last_x, mill_last_y, i_x, i_y);
								double i_x2 = i_x;
								double i_y2 = i_y;
								add_angle_offset(&i_x2, &i_y2, (PARAMETER[P_T_LEN].vdouble + PARAMETER[P_TOOL_DIAMETER].vdouble) / 2.0, alpha1 + 180);
								double alpha2 = vector_angle(x, y, i_x, i_y);
								double i_x3 = i_x;
								double i_y3 = i_y;
								add_angle_offset(&i_x3, &i_y3, (PARAMETER[P_T_LEN].vdouble + PARAMETER[P_TOOL_DIAMETER].vdouble) / 2.0, alpha2 + 180);
								draw_line((float)mill_last_x, (float)mill_last_y, (float)mill_last_z, (float)i_x2, (float)i_y2, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
								draw_line((float)i_x2, (float)i_y2, (float)PARAMETER[P_T_DEPTH].vdouble, (float)i_x3, (float)i_y3, PARAMETER[P_T_DEPTH].vdouble, PARAMETER[P_TOOL_DIAMETER].vdouble);
								hflag = 1;
								postcam_var_push_double("endX", _X(i_x2));
								postcam_var_push_double("endY", _Y(i_y2));
								postcam_call_function("OnMove");
								postcam_var_push_double("currentX", _X(i_x2));
								postcam_var_push_double("currentY", _Y(i_y2));
								if (PARAMETER[P_T_TYPE].vint == 0) {
									postcam_var_push_double("endZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_var_push_double("endX", _X(i_x3));
									postcam_var_push_double("endY", _Y(i_y3));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x3));
									postcam_var_push_double("currentY", _Y(i_y3));
									postcam_var_push_double("endZ", _Z(mill_last_z));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentZ", _Z(mill_last_z));
								} else {
									postcam_var_push_double("endX", _X(i_x));
									postcam_var_push_double("endY", _Y(i_y));
									postcam_var_push_double("endZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x));
									postcam_var_push_double("currentY", _Y(i_y));
									postcam_var_push_double("currentZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_var_push_double("endX", _X(i_x3));
									postcam_var_push_double("endY", _Y(i_y3));
									postcam_var_push_double("endZ", _Z(mill_last_z));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x3));
									postcam_var_push_double("currentY", _Y(i_y3));
									postcam_var_push_double("currentZ", _Z(mill_last_z));
								}
								mill_last_x = i_x3;
								mill_last_y = i_y3;
							}
						} else {
							break;
						}
					}
				} else {
					if (PARAMETER[P_T_XGRID].vdouble > 0.0) {
						double nx = 0.0;
						double tx1 = 0.0;
						double ty1 = -10.0;
						double tx2 = size_x;
						double ty2 = size_y + 10.0;
						int nn = (int)size_x / (int)PARAMETER[P_T_XGRID].vdouble;
						double maxn = nn * PARAMETER[P_T_XGRID].vdouble;
						while (1) {
							if (nx > size_x) {
								break;
							}
							if (mill_last_x < x) {
								tx1 = nx;
								tx2 = nx;
							} else {
								tx1 = maxn - nx;
								tx2 = maxn - nx;
							}
							glColor4f(0.0, 0.0, 1.0, 0.1);
							glBegin(GL_LINES);
							glVertex3f(tx1, ty1, PARAMETER[P_T_DEPTH].vdouble);
							glVertex3f(tx2, ty2, PARAMETER[P_T_DEPTH].vdouble);
							glEnd();
							if (mill_last_z < PARAMETER[P_T_DEPTH].vdouble && (intersect_check(mill_last_x, mill_last_y, x, y, tx1 + 0.0002, ty1 + 0.0002, tx2 + 0.0002, ty2 + 0.0002, &i_x, &i_y) == 1 || intersect_check(x, y, mill_last_x, mill_last_y, tx1 + 0.0002, ty1 + 0.0002, tx2 + 0.0002, ty2 + 0.0002, &i_x, &i_y) == 1)) {
								double alpha1 = vector_angle(mill_last_x, mill_last_y, i_x, i_y);
								double i_x2 = i_x;
								double i_y2 = i_y;
								add_angle_offset(&i_x2, &i_y2, (PARAMETER[P_T_LEN].vdouble + PARAMETER[P_TOOL_DIAMETER].vdouble) / 2.0, alpha1 + 180);
								double dist = set_positive(get_len(mill_last_x, mill_last_y, i_x, i_y));
								double dist2 = set_positive(get_len(x, y, i_x, i_y));
								if (dist < PARAMETER[P_T_LEN].vdouble || dist2 < PARAMETER[P_T_LEN].vdouble) {
									nx += PARAMETER[P_T_XGRID].vdouble;
									continue;
								}
								double alpha2 = vector_angle(x, y, i_x, i_y);
								double i_x3 = i_x;
								double i_y3 = i_y;
								add_angle_offset(&i_x3, &i_y3, (PARAMETER[P_T_LEN].vdouble + PARAMETER[P_TOOL_DIAMETER].vdouble) / 2.0, alpha2 + 180);
								draw_line((float)mill_last_x, (float)mill_last_y, (float)mill_last_z, (float)i_x2, (float)i_y2, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
								draw_line((float)i_x2, (float)i_y2, (float)PARAMETER[P_T_DEPTH].vdouble, (float)i_x3, (float)i_y3, PARAMETER[P_T_DEPTH].vdouble, PARAMETER[P_TOOL_DIAMETER].vdouble);
								hflag = 1;
								postcam_var_push_double("endX", _X(i_x2));
								postcam_var_push_double("endY", _Y(i_y2));
								postcam_call_function("OnMove");
								postcam_var_push_double("currentX", _X(i_x2));
								postcam_var_push_double("currentY", _Y(i_y2));
								if (PARAMETER[P_T_TYPE].vint == 0) {
									glColor4f(1.0, 1.0, 0.0, 0.5);
									glBegin(GL_QUADS);
									glVertex3f(i_x2, i_y2 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x2, i_y2 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2, i_y2 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2, i_y2 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glEnd();
									glBegin(GL_QUADS);
									glVertex3f(i_x2, i_y2 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2, i_y2 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glEnd();
									glBegin(GL_QUADS);
									glVertex3f(i_x3, i_y3 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glEnd();
									postcam_var_push_double("endZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_var_push_double("endX", _X(i_x3));
									postcam_var_push_double("endY", _Y(i_y3));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x3));
									postcam_var_push_double("currentY", _Y(i_y3));
									postcam_var_push_double("endZ", _Z(mill_last_z));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentZ", _Z(mill_last_z));
								} else {
									glColor4f(1.0, 1.0, 0.0, 0.5);
									glBegin(GL_QUADS);
									glVertex3f(i_x2, i_y2 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x, i_y - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x, i_y + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2, i_y2 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glEnd();
									glBegin(GL_QUADS);
									glVertex3f(i_x, i_y - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x3, i_y3 + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x, i_y + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_T_DEPTH].vdouble);
									glEnd();
									postcam_var_push_double("endX", _X(i_x));
									postcam_var_push_double("endY", _Y(i_y));
									postcam_var_push_double("endZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x));
									postcam_var_push_double("currentY", _Y(i_y));
									postcam_var_push_double("currentZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_var_push_double("endX", _X(i_x3));
									postcam_var_push_double("endY", _Y(i_y3));
									postcam_var_push_double("endZ", _Z(mill_last_z));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x3));
									postcam_var_push_double("currentY", _Y(i_y3));
									postcam_var_push_double("currentZ", _Z(mill_last_z));
								}
								mill_last_x = i_x3;
								mill_last_y = i_y3;
							}
							nx += PARAMETER[P_T_XGRID].vdouble;
						}
					}
					if (PARAMETER[P_T_YGRID].vdouble > 0.0) {
						double ny = 0.0;
						double tx1 = -10.0;
						double ty1 = 0.0;
						double tx2 = size_x + 10.0;
						double ty2 = size_y;
						int nn = (int)size_y / (int)PARAMETER[P_T_YGRID].vdouble;
						double maxn = nn * PARAMETER[P_T_YGRID].vdouble;
						while (1) {
							if (ny > size_y) {
								break;
							}
							if (mill_last_y < y) {
								ty1 = ny;
								ty2 = ny;
							} else {
								ty1 = maxn - ny;
								ty2 = maxn - ny;
							}
							glColor4f(0.0, 0.0, 1.0, 0.1);
							glBegin(GL_LINES);
							glVertex3f(tx1, ty1, PARAMETER[P_T_DEPTH].vdouble);
							glVertex3f(tx2, ty2, PARAMETER[P_T_DEPTH].vdouble);
							glEnd();
							if (mill_last_z < PARAMETER[P_T_DEPTH].vdouble && (intersect_check(mill_last_x, mill_last_y, x, y, tx1 + 0.0002, ty1 + 0.0002, tx2 + 0.0002, ty2 + 0.0002, &i_x, &i_y) == 1 || intersect_check(x, y, mill_last_x, mill_last_y, tx1 + 0.0002, ty1 + 0.0002, tx2 + 0.0002, ty2 + 0.0002, &i_x, &i_y) == 1)) {
								double alpha1 = vector_angle(mill_last_x, mill_last_y, i_x, i_y);
								double i_x2 = i_x;
								double i_y2 = i_y;
								add_angle_offset(&i_x2, &i_y2, (PARAMETER[P_T_LEN].vdouble + PARAMETER[P_TOOL_DIAMETER].vdouble) / 2.0, alpha1 + 180);
								double dist = set_positive(get_len(mill_last_x, mill_last_y, i_x, i_y));
								double dist2 = set_positive(get_len(x, y, i_x, i_y));
								if (dist < PARAMETER[P_T_LEN].vdouble || dist2 < PARAMETER[P_T_LEN].vdouble) {
									ny += PARAMETER[P_T_YGRID].vdouble;
									continue;
								}
								double alpha2 = vector_angle(x, y, i_x, i_y);
								double i_x3 = i_x;
								double i_y3 = i_y;
								add_angle_offset(&i_x3, &i_y3, (PARAMETER[P_T_LEN].vdouble + PARAMETER[P_TOOL_DIAMETER].vdouble) / 2.0, alpha2 + 180);
								draw_line((float)mill_last_x, (float)mill_last_y, (float)mill_last_z, (float)i_x2, (float)i_y2, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
								draw_line((float)i_x2, (float)i_y2, (float)PARAMETER[P_T_DEPTH].vdouble, (float)i_x3, (float)i_y3, PARAMETER[P_T_DEPTH].vdouble, PARAMETER[P_TOOL_DIAMETER].vdouble);
								hflag = 1;
								postcam_var_push_double("endX", _X(i_x2));
								postcam_var_push_double("endY", _Y(i_y2));
								postcam_call_function("OnMove");
								postcam_var_push_double("currentX", _X(i_x2));
								postcam_var_push_double("currentY", _Y(i_y2));
								if (PARAMETER[P_T_TYPE].vint == 0) {

									glColor4f(1.0, 1.0, 0.0, 0.5);
									glBegin(GL_QUADS);
									glVertex3f(i_x2 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x2 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_M_DEPTH].vdouble);
									glEnd();
									glBegin(GL_QUADS);
									glVertex3f(i_x2 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_T_DEPTH].vdouble);
									glEnd();
									glBegin(GL_QUADS);
									glVertex3f(i_x3 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x3 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x3 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_T_DEPTH].vdouble);
									glEnd();


									postcam_var_push_double("endZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_var_push_double("endX", _X(i_x3));
									postcam_var_push_double("endY", _Y(i_y3));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x3));
									postcam_var_push_double("currentY", _Y(i_y3));
									postcam_var_push_double("endZ", _Z(mill_last_z));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentZ", _Z(mill_last_z));
								} else {
									glColor4f(1.0, 1.0, 0.0, 0.5);
									glBegin(GL_QUADS);
									glVertex3f(i_x2 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x2 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y2, PARAMETER[P_M_DEPTH].vdouble);
									glEnd();
									glBegin(GL_QUADS);
									glVertex3f(i_x - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y, PARAMETER[P_T_DEPTH].vdouble);
									glVertex3f(i_x3 - PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x3 + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y3, PARAMETER[P_M_DEPTH].vdouble);
									glVertex3f(i_x + PARAMETER[P_TOOL_DIAMETER].vdouble, i_y, PARAMETER[P_T_DEPTH].vdouble);
									glEnd();
									postcam_var_push_double("endX", _X(i_x));
									postcam_var_push_double("endY", _Y(i_y));
									postcam_var_push_double("endZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x));
									postcam_var_push_double("currentY", _Y(i_y));
									postcam_var_push_double("currentZ", _Z(PARAMETER[P_T_DEPTH].vdouble));
									postcam_var_push_double("endX", _X(i_x3));
									postcam_var_push_double("endY", _Y(i_y3));
									postcam_var_push_double("endZ", _Z(mill_last_z));
									postcam_call_function("OnMove");
									postcam_var_push_double("currentX", _X(i_x3));
									postcam_var_push_double("currentY", _Y(i_y3));
									postcam_var_push_double("currentZ", _Z(mill_last_z));
								}
								mill_last_x = i_x3;
								mill_last_y = i_y3;
							}
							ny += PARAMETER[P_T_YGRID].vdouble;
						}
					}
				}
			}
		}
		draw_line((float)mill_last_x, (float)mill_last_y, (float)mill_last_z, (float)x, (float)y, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
		if (gcmd == 1) {
#ifdef USE_POSTCAM
			postcam_var_push_double("endX", _X(x));
			postcam_var_push_double("endY", _Y(y));
			postcam_call_function("OnMove");
#else
			translateAxisX(x, tx_str);
			translateAxisY(y, ty_str);
			sprintf(cline, "G0%i %s %s F%i\n", gcmd, tx_str, ty_str, feed);
			append_gcode(cline);
#endif
		} else if (gcmd == 2 || gcmd == 3) {
#ifdef USE_POSTCAM
			postcam_var_push_double("endX", _X(x));
			postcam_var_push_double("endY", _Y(y));
			double e = x - mill_last_x;
			double f = y - mill_last_y;
			double p = sqrt(e*e + f*f);
			double k = (p*p + r*r - r*r) / (2 * p);
			if (gcmd == 2) {
				double cx = mill_last_x + e * k/p + (f/p) * sqrt(r * r - k * k);
				double cy = mill_last_y + f * k/p - (e/p) * sqrt(r * r - k * k);
				double dx1 = cx - mill_last_x;
				double dy1 = cy - mill_last_y;
				double alpha1 = toDeg(atan2(dx1, dy1)) + 180.0;
				double dx2 = cx - x;
				double dy2 = cy - y;
				double alpha2 = toDeg(atan2(dx2, dy2)) + 180.0;
				double alpha = alpha2 - alpha1;
				if (alpha >= 360.0) {
					alpha -= 360.0;
				}
				if (alpha < 0.0) {
					alpha += 360.0;
				}
				postcam_var_push_double("arcCentreX", _X(cx));
				postcam_var_push_double("arcCentreY", _Y(cy));
				postcam_var_push_double("arcAngle", toRad(alpha));
			} else {
				double cx = mill_last_x + e * k/p - (f/p) * sqrt(r * r - k * k);
				double cy = mill_last_y + f * k/p + (e/p) * sqrt(r * r - k * k);
				double dx1 = cx - mill_last_x;
				double dy1 = cy - mill_last_y;
				double alpha1 = toDeg(atan2(dx1, dy1)) + 180.0;
				double dx2 = cx - x;
				double dy2 = cy - y;
				double alpha2 = toDeg(atan2(dx2, dy2)) + 180.0;
				double alpha = alpha2 - alpha1;
				if (alpha <= -360.0) {
					alpha += 360.0;
				}
				if (alpha > 0.0) {
					alpha -= 360.0;
				}
				postcam_var_push_double("arcCentreX", _X(cx));
				postcam_var_push_double("arcCentreY", _Y(cy));
				postcam_var_push_double("arcAngle", toRad(alpha));
			}
			postcam_var_push_double("arcRadius", r);
			postcam_call_function("OnArc");
#else
			translateAxisX(x, tx_str);
			translateAxisY(y, ty_str);
			sprintf(cline, "G0%i %s %s R%f F%i\n", gcmd, tx_str, ty_str, r, feed);
			append_gcode(cline);
#endif
		}
	} else {
		if (mill_start_all != 0) {
			glColor4f(0.0, 1.0, 1.0, 1.0);
			draw_line3((float)mill_last_x, (float)mill_last_y, (float)mill_last_z, (float)x, (float)y, (float)mill_last_z);
		}
#ifdef USE_POSTCAM
		postcam_var_push_int("feedRate", feed);
		postcam_var_push_double("currentX", _X(mill_last_x));
		postcam_var_push_double("currentY", _Y(mill_last_y));
		postcam_var_push_double("currentZ", _Z(mill_last_z));
		postcam_var_push_double("endX", _X(x));
		postcam_var_push_double("endY", _Y(y));
		postcam_var_push_double("endZ", _Z(mill_last_z));
		postcam_call_function("OnRapid");
#else
		translateAxisX(x, tx_str);
		translateAxisY(y, ty_str);
		sprintf(cline, "G00 %s %s\n", tx_str, ty_str);
		append_gcode(cline);
#endif
	}
	if (gcmd == 0) {
		move_distance_xy += set_positive(get_len(mill_last_x, mill_last_y, x, y));
	} else {
		mill_distance_xy += set_positive(get_len(mill_last_x, mill_last_y, x, y));
	}

	mill_start_all = 1;
	mill_last_x = x;
	mill_last_y = y;
}

void mill_drill (double x, double y, double depth, int feed, char *comment) {
	if (comment[0] != 0) {
		sprintf(cline, "(%s)\n", comment);
		append_gcode(cline);
	}
	mill_z(1, depth);
	draw_line(x, y, (float)mill_last_z, (float)x, (float)y, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
	mill_z(0, 0.0);
}

void mill_circle (int gcmd, double x, double y, double r, double depth, int feed, int inside, char *comment) {
#ifndef USE_POSTCAM
	char tx_str[128];
	char ty_str[128];
#endif
	if (comment[0] != 0) {
		sprintf(cline, "(%s)\n", comment);
		append_gcode(cline);
	}
	mill_z(1, depth);
#ifdef USE_POSTCAM
	postcam_var_push_int("feedRate", feed);
	postcam_var_push_double("currentX", _X(mill_last_x));
	postcam_var_push_double("currentY", _Y(mill_last_y));
	postcam_var_push_double("currentZ", _Z(mill_last_z));
	postcam_var_push_double("endX", _X(x + r));
	postcam_var_push_double("endY", _Y(y));
	postcam_var_push_double("endZ", _Z(mill_last_z));
	postcam_var_push_double("arcRadius", r);
	postcam_var_push_double("arcCentreX", _X(x));
	postcam_var_push_double("arcCentreY", _Y(y));
	if (gcmd == 2) {
		postcam_var_push_double("arcAngle", 360.0);
	} else {
		postcam_var_push_double("arcAngle", -360.0);
	}
	postcam_call_function("OnArc");
	postcam_var_push_double("currentX", _X(x + r));
	postcam_var_push_double("endX", _X(x - r));
	postcam_call_function("OnArc");
#else
	translateAxisX(x + r, tx_str);
	translateAxisY(y, ty_str);
	sprintf(cline, "G0%i %s %s R%f F%i\n", gcmd, tx_str, ty_str, r, feed);
	append_gcode(cline);
	translateAxisX(x - r, tx_str);
	translateAxisY(y, ty_str);
	sprintf(cline, "G0%i %s %s R%f F%i\n", gcmd, tx_str, ty_str, r, feed);
	append_gcode(cline);
#endif
	float an = 0.0;
	float last_x = x + r;
	float last_y = y;
	for (an = 0.0; an <= 360.0; an += 18.0) {
		float angle1 = toRad(an);
		float x1 = r * cos(angle1);
		float y1 = r * sin(angle1);
		if (inside == 1) {
			if (gcmd == 3) {
				draw_line(last_x, last_y, (float)mill_last_z, (float)x + x1, (float)y + y1, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
			} else {
				draw_line((float)x + x1, (float)y + y1, (float)mill_last_z, last_x, last_y, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
			}
		} else {
			if (gcmd == 2) {
				draw_line(last_x, last_y, (float)mill_last_z, (float)x + x1, (float)y + y1, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
			} else {
				draw_line((float)x + x1, (float)y + y1, (float)mill_last_z, last_x, last_y, (float)mill_last_z, PARAMETER[P_TOOL_DIAMETER].vdouble);
			}
		}
		last_x = (float)x + x1;
		last_y = (float)y + y1;
	}
	mill_distance_xy += set_positive(r * 2 * PI);
}

void mill_move_in (double x, double y, double depth, int lasermode) {
	// move to
	if (lasermode == 1) {
		if (tool_last != 5) {
#ifdef USE_POSTCAM
			postcam_var_push_double("tool", PARAMETER[P_TOOL_NUM].vint);
			char tmp_str[1024];
			sprintf(tmp_str, "Tool# %i", PARAMETER[P_TOOL_NUM].vint);
			postcam_var_push_string("toolName", tmp_str);
			postcam_var_push_int("spindleSpeed", PARAMETER[P_TOOL_SPEED].vint);
			postcam_call_function("OnToolChange");
#else
			sprintf(cline, "M6 T%i", PARAMETER[P_TOOL_NUM].vint);
			append_gcode(cline);
#endif
		}
		tool_last = PARAMETER[P_TOOL_NUM].vint;
		mill_xy(0, x, y, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
		mill_z(0, 0.0);
#ifdef USE_POSTCAM
		postcam_call_function("OnSpindleCW");
#else
		sprintf(cline, "M03 (Laser-On)\n");
		append_gcode(cline);
#endif
	} else {
		if (tool_last != PARAMETER[P_TOOL_NUM].vint) {
#ifdef USE_POSTCAM
			postcam_var_push_double("tool", PARAMETER[P_TOOL_NUM].vint);
			char tmp_str[1024];
			sprintf(tmp_str, "Tool# %i", PARAMETER[P_TOOL_NUM].vint);
			postcam_var_push_string("toolName", tmp_str);
			postcam_var_push_double("endZ", _Z(mill_last_z));
			postcam_var_push_int("spindleSpeed", PARAMETER[P_TOOL_SPEED].vint);
			postcam_call_function("OnToolChange");
			postcam_call_function("OnSpindleCW");
#else
			sprintf(cline, "M6 T%i", PARAMETER[P_TOOL_NUM].vint);
			append_gcode(cline);
			sprintf(cline, "M03 S%i", PARAMETER[P_TOOL_SPEED].vint);
			append_gcode(cline);
#endif
		}
		tool_last = PARAMETER[P_TOOL_NUM].vint;
		mill_z(0, PARAMETER[P_CUT_SAVE].vdouble);
		mill_xy(0, x, y, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
	}
}


void mill_move_out (int lasermode) {
	// move out
	if (lasermode == 1) {
#ifdef USE_POSTCAM
		postcam_call_function("OnSpindleOff");
#else
		sprintf(cline, "M05 (Laser-Off)\n");
		append_gcode(cline);
#endif
	} else {
		mill_z(0, PARAMETER[P_CUT_SAVE].vdouble);
	}
}

void object_draw (FILE *fd_out, int object_num) {
	int num = 0;
	int last = 0;
	int lasermode = 0;
	double mill_depth_real = 0.0;
	char tmp_str[1024];

	lasermode = myOBJECTS[object_num].laser;
	mill_depth_real = myOBJECTS[object_num].depth;

	/* find last line in object */
	for (num = 0; num < line_last; num++) {
		if (myOBJECTS[object_num].line[num] != 0) {
			last = myOBJECTS[object_num].line[num];
		}
	}
	if (PARAMETER[P_O_SELECT].vint == object_num) {
		glColor4f(1.0, 0.0, 0.0, 0.3);
		glBegin(GL_QUADS);
		glVertex3f(myOBJECTS[object_num].min_x - PARAMETER[P_TOOL_DIAMETER].vdouble, myOBJECTS[object_num].min_y - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
		glVertex3f(myOBJECTS[object_num].max_x + PARAMETER[P_TOOL_DIAMETER].vdouble, myOBJECTS[object_num].min_y - PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
		glVertex3f(myOBJECTS[object_num].max_x + PARAMETER[P_TOOL_DIAMETER].vdouble, myOBJECTS[object_num].max_y + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
		glVertex3f(myOBJECTS[object_num].min_x - PARAMETER[P_TOOL_DIAMETER].vdouble, myOBJECTS[object_num].max_y + PARAMETER[P_TOOL_DIAMETER].vdouble, PARAMETER[P_M_DEPTH].vdouble);
		glEnd();
	}
	if (PARAMETER[P_M_NCDEBUG].vint == 1) {
		append_gcode("\n");
		sprintf(cline, "(--------------------------------------------------)\n");
		append_gcode(cline);
		sprintf(cline, "(Object: #%i)\n", object_num);
		append_gcode(cline);
		sprintf(cline, "(Layer: %s)\n", myOBJECTS[object_num].layer);
		append_gcode(cline);
		if (lasermode == 1) {
			sprintf(cline, "(Laser-Mode: On)\n");
		} else { 
			sprintf(cline, "(Depth: %f)\n", mill_depth_real);
		}
		append_gcode(cline);
		append_gcode("(--------------------------------------------------)\n");
		append_gcode("\n");
	}
	if (myLINES[myOBJECTS[object_num].line[0]].type == TYPE_CIRCLE) {
		int lnum = myOBJECTS[object_num].line[0];
		double an = 0.0;
		double r = (float)myLINES[lnum].opt;
		double x = (float)myLINES[lnum].cx;
		double y = (float)myLINES[lnum].cy;
		double last_x = x + r;
		double last_y = y;

		if (PARAMETER[P_M_NCDEBUG].vint == 1) {
			mill_move_in(myLINES[lnum].cx - r, myLINES[lnum].cy, 0.0, lasermode);
			mill_circle(2, myLINES[lnum].cx, myLINES[lnum].cy, r, 0.0, PARAMETER[P_M_FEEDRATE].vint, myOBJECTS[object_num].inside, "");
		}
		for (an = 0.0; an <= 360.0; an += 9.0) {
			double angle1 = toRad(an);
			double x1 = r * cos(angle1);
			double y1 = r * sin(angle1);
			if (PARAMETER[P_O_SELECT].vint == object_num) {
				glColor4f(1.0, 0.0, 0.0, 1.0);
			} else {
				glColor4f(0.0, 1.0, 0.0, 1.0);
			}
			draw_oline(last_x, last_y, (float)x + x1, (float)y + y1, mill_depth_real);
			last_x = (float)x + x1;
			last_y = (float)y + y1;
		}
		if (PARAMETER[P_M_ROTARYMODE].vint == 0) {
			if (object_num == PARAMETER[P_O_SELECT].vint) {
				glColor4f(1.0, 0.0, 0.0, 1.0);
			} else {
				glColor4f(1.0, 1.0, 1.0, 1.0);
			}
			sprintf(tmp_str, "%i", object_num);
			output_text_gl_center(tmp_str, (float)x + (float)r, (float)y, PARAMETER[P_CUT_SAVE].vdouble, 0.2);
			if (PARAMETER[P_V_HELPLINES].vint == 1) {
				if (myOBJECTS[object_num].closed == 1 && myOBJECTS[object_num].inside == 0) {
					object2poly(object_num, 0.0, mill_depth_real, 0);
				} else if (myOBJECTS[object_num].inside == 1 && mill_depth_real > PARAMETER[P_M_DEPTH].vdouble) {
					object2poly(object_num, mill_depth_real - 0.001, mill_depth_real - 0.001, 1);
				}
			}
		}
		return;
	}
	for (num = 0; num < line_last; num++) {
		if (myOBJECTS[object_num].line[num] != 0) {
			int lnum = myOBJECTS[object_num].line[num];
			if (PARAMETER[P_O_SELECT].vint == object_num) {
				glColor4f(1.0, 0.0, 0.0, 1.0);
			} else {
				glColor4f(0.0, 1.0, 0.0, 1.0);
			}
			draw_oline((float)myLINES[lnum].x1, (float)myLINES[lnum].y1, (float)myLINES[lnum].x2, (float)myLINES[lnum].y2, mill_depth_real);
			if (myOBJECTS[object_num].closed == 0 && (myLINES[lnum].type != TYPE_MTEXT || PARAMETER[P_M_TEXT].vint == 1)) {
				draw_line2((float)myLINES[lnum].x1, (float)myLINES[lnum].y1, 0.01, (float)myLINES[lnum].x2, (float)myLINES[lnum].y2, 0.01, (PARAMETER[P_TOOL_DIAMETER].vdouble));
			}
			if (PARAMETER[P_M_NCDEBUG].vint == 1) {
				if (num == 0) {
					if (lasermode == 1) {
						if (tool_last != 5) {
							sprintf(cline, "M06 T%i (Change-Tool / Laser-Mode)\n", 5);
							append_gcode(cline);
						}
						tool_last = 5;
					}
					mill_xy(0, myLINES[lnum].x1, myLINES[lnum].y1, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
					if (lasermode == 1) {
						mill_z(0, 0.0);
						sprintf(cline, "M03 (Laser-On)\n");
						append_gcode(cline);
					}
				}
				if (myLINES[lnum].type == TYPE_ARC || myLINES[lnum].type == TYPE_CIRCLE) {
					if (myOBJECTS[object_num].climb == 0) {
						if (myLINES[lnum].opt < 0) {
							mill_xy(3, myLINES[lnum].x2, myLINES[lnum].y2, myLINES[lnum].opt * -1, PARAMETER[P_M_FEEDRATE].vint, "");
						} else {
							mill_xy(2, myLINES[lnum].x2, myLINES[lnum].y2, myLINES[lnum].opt, PARAMETER[P_M_FEEDRATE].vint, "");
						}
					} else {
						if (myLINES[lnum].opt < 0) {
							mill_xy(2, myLINES[lnum].x2, myLINES[lnum].y2, myLINES[lnum].opt * -1, PARAMETER[P_M_FEEDRATE].vint, "");
						} else {
							mill_xy(3, myLINES[lnum].x2, myLINES[lnum].y2, myLINES[lnum].opt, PARAMETER[P_M_FEEDRATE].vint, "");
						}
					}
				} else if (myLINES[lnum].type == TYPE_MTEXT) {
					if (PARAMETER[P_M_TEXT].vint == 1) {
						mill_xy(1, myLINES[lnum].x2, myLINES[lnum].y2, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
					}
				} else {
					mill_xy(1, myLINES[lnum].x2, myLINES[lnum].y2, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
				}
			}
			if (num == 0) {
				if (myLINES[lnum].type != TYPE_MTEXT || PARAMETER[P_M_TEXT].vint == 1) {
					if (PARAMETER[P_M_ROTARYMODE].vint == 0) {
						if (object_num == PARAMETER[P_O_SELECT].vint) {
							glColor4f(1.0, 0.0, 0.0, 1.0);
						} else {
							glColor4f(1.0, 1.0, 1.0, 1.0);
						}
						sprintf(tmp_str, "%i", object_num);
						output_text_gl_center(tmp_str, (float)myLINES[lnum].x1, (float)myLINES[lnum].y1, PARAMETER[P_CUT_SAVE].vdouble, 0.2);
					}
				}
			}
		}
	}
	if (PARAMETER[P_M_ROTARYMODE].vint == 0) {
		if (PARAMETER[P_V_HELPLINES].vint == 1) {
			if (myOBJECTS[object_num].closed == 1 && myOBJECTS[object_num].inside == 0) {
				object2poly(object_num, 0.0, mill_depth_real, 0);
			} else if (myOBJECTS[object_num].inside == 1 && mill_depth_real > PARAMETER[P_M_DEPTH].vdouble) {
				object2poly(object_num, mill_depth_real - 0.001, mill_depth_real - 0.001, 1);
			}
		}
	}
}

void object_draw_offset_depth (FILE *fd_out, int object_num, double depth, double *next_x, double *next_y, double tool_offset, int overcut, int lasermode, int offset) {
	int error = 0;
	int lnum1 = 0;
	int lnum2 = 0;
	int num = 0;
	int last = 0;
	int last_lnum = 0;
	double first_x = 0.0;
	double first_y = 0.0;
	double last_x = 0.0;
	double last_y = 0.0;

	/* find last line in object */
	for (num = 0; num < line_last; num++) {
		if (myOBJECTS[object_num].line[num] != 0) {
			last = myOBJECTS[object_num].line[num];
		}
	}
	if (myLINES[last].type == TYPE_MTEXT && PARAMETER[P_M_TEXT].vint == 0) {
		return;
	}

	if (myLINES[myOBJECTS[object_num].line[0]].type == TYPE_CIRCLE) {
		int lnum = myOBJECTS[object_num].line[0];
		double r = myLINES[lnum].opt;
		if (r < 0.0) {
			r *= -1;
		}
		if (r > PARAMETER[P_TOOL_DIAMETER].vdouble / 2.0) {
			if (offset == 1) {
				r -= tool_offset;
			} else if (offset == 2) {
				r += tool_offset;
			}
			if (mill_start == 0) {
				mill_move_in(myLINES[lnum].cx - r, myLINES[lnum].cy, depth, lasermode);
				mill_start = 1;
			}
			if (myOBJECTS[object_num].climb == 0) {
				mill_circle(2, myLINES[lnum].cx, myLINES[lnum].cy, r, depth, PARAMETER[P_M_FEEDRATE].vint, myOBJECTS[object_num].inside, "");
			} else {
				mill_circle(3, myLINES[lnum].cx, myLINES[lnum].cy, r, depth, PARAMETER[P_M_FEEDRATE].vint, myOBJECTS[object_num].inside, "");
			}
			*next_x = myLINES[lnum].cx - r;
			*next_y = myLINES[lnum].cy;
		} else {

			if (mill_start == 0) {
				mill_move_in(myLINES[lnum].cx, myLINES[lnum].cy, depth, lasermode);
				mill_start = 1;
			}
			mill_drill(myLINES[lnum].cx, myLINES[lnum].cy, depth, PARAMETER[P_M_FEEDRATE].vint, "");
			*next_x = myLINES[lnum].cx;
			*next_y = myLINES[lnum].cy;
		}
		return;
	}

	for (num = 0; num < line_last; num++) {
		if (myOBJECTS[object_num].line[num] != 0) {
			if (num == 0) {
				lnum1 = last;
			} else {
				lnum1 = last_lnum;
			}
			lnum2 = myOBJECTS[object_num].line[num];
			if (myOBJECTS[object_num].closed == 1 && offset != 0) {
				// line1 Offsets & Angle
				double alpha1 = line_angle(lnum1);
				double check1_x = myLINES[lnum1].x1;
				double check1_y = myLINES[lnum1].y1;
				double check1b_x = myLINES[lnum1].x2;
				double check1b_y = myLINES[lnum1].y2;
				add_angle_offset(&check1b_x, &check1b_y, 0.0, alpha1);
				if (myOBJECTS[object_num].climb == 0) {
					add_angle_offset(&check1_x, &check1_y, -tool_offset, alpha1 + 90);
					add_angle_offset(&check1b_x, &check1b_y, -tool_offset, alpha1 + 90);
				} else {
					add_angle_offset(&check1_x, &check1_y, tool_offset, alpha1 + 90);
					add_angle_offset(&check1b_x, &check1b_y, tool_offset, alpha1 + 90);
				}

				// line2 Offsets & Angle
				double alpha2 = line_angle(lnum2);
				double check2_x = myLINES[lnum2].x1;
				double check2_y = myLINES[lnum2].y1;
				add_angle_offset(&check2_x, &check2_y, 0.0, alpha2);
				double check2b_x = myLINES[lnum2].x2;
				double check2b_y = myLINES[lnum2].y2;
				if (myOBJECTS[object_num].climb == 0) {
					add_angle_offset(&check2_x, &check2_y, -tool_offset, alpha2 + 90);
					add_angle_offset(&check2b_x, &check2b_y, -tool_offset, alpha2 + 90);
				} else {
					add_angle_offset(&check2_x, &check2_y, tool_offset, alpha2 + 90);
					add_angle_offset(&check2b_x, &check2b_y, tool_offset, alpha2 + 90);
				}

				// Angle-Diff
				alpha1 = alpha1 + 180.0;
				alpha2 = alpha2 + 180.0;
				double alpha_diff = alpha2 - alpha1;
				if (alpha_diff < 0.0) {
					alpha_diff += 360.0;
				}
				if (alpha_diff > 360.0) {
					alpha_diff -= 360.0;
				}
				alpha1 = line_angle2(lnum1);
				alpha2 = line_angle2(lnum2);
				alpha_diff = alpha2 - alpha1;
				if (alpha_diff > 180.0) {
					alpha_diff -= 360.0;
				}
				if (alpha_diff < -180.0) {
					alpha_diff += 360.0;
				}
				if (alpha_diff == 0.0) {
				} else if ((myOBJECTS[object_num].climb == 1 && alpha_diff > 0.0) || (myOBJECTS[object_num].climb == 0 && alpha_diff < 0.0)) {
					// Aussenkante
					if (num == 0) {
						first_x = check1b_x;
						first_y = check1b_y;
						if (mill_start == 0) {
							mill_move_in(first_x, first_y, depth, lasermode);
							mill_start = 1;
						}
						append_gcode("\n");
						mill_z(1, depth);

						if (myOBJECTS[object_num].climb == 0) {
							mill_xy(3, check2_x, check2_y, tool_offset, PARAMETER[P_M_FEEDRATE].vint, "");
						} else {
							mill_xy(2, check2_x, check2_y, tool_offset, PARAMETER[P_M_FEEDRATE].vint, "");
						}
					} else {
						if (myLINES[lnum1].type == TYPE_ARC || myLINES[lnum1].type == TYPE_CIRCLE) {
							if (myOBJECTS[object_num].climb == 0) {
								if (myLINES[lnum1].opt < 0) {
									mill_xy(2, check1b_x, check1b_y, (myLINES[lnum1].opt + tool_offset) * -1, PARAMETER[P_M_FEEDRATE].vint, "");
								} else {
									mill_xy(3, check1b_x, check1b_y, (myLINES[lnum1].opt + tool_offset), PARAMETER[P_M_FEEDRATE].vint, "");
								}
							} else {
								if (myLINES[lnum1].opt < 0) {
									mill_xy(2, check1b_x, check1b_y, (myLINES[lnum1].opt - tool_offset) * -1, PARAMETER[P_M_FEEDRATE].vint, "");
								} else {
									mill_xy(3, check1b_x, check1b_y, (myLINES[lnum1].opt - tool_offset), PARAMETER[P_M_FEEDRATE].vint, "");
								}
							}
							if (myLINES[lnum2].type == TYPE_ARC || myLINES[lnum2].type == TYPE_CIRCLE) {
							} else {
								if (myOBJECTS[object_num].climb == 0) {
									mill_xy(3, check2_x, check2_y, tool_offset, PARAMETER[P_M_FEEDRATE].vint, "");
								} else {
									mill_xy(2, check2_x, check2_y, tool_offset, PARAMETER[P_M_FEEDRATE].vint, "");
								}
							}
						} else {
							mill_xy(1, check1b_x, check1b_y, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
							if (myOBJECTS[object_num].climb == 0) {
								mill_xy(3, check2_x, check2_y, tool_offset, PARAMETER[P_M_FEEDRATE].vint, "");
							} else {
								mill_xy(2, check2_x, check2_y, tool_offset, PARAMETER[P_M_FEEDRATE].vint, "");
							}
						}
					}
					last_x = check2_x;
					last_y = check2_y;
				} else {
					// Innenkante
					if (myLINES[lnum2].type == TYPE_ARC || myLINES[lnum2].type == TYPE_CIRCLE) {
						if (myLINES[lnum2].opt < 0 && myLINES[lnum2].opt * -1 < tool_offset) {
							error = 1;
							break;
						}
					}
					double px = 0.0;
					double py = 0.0;
					intersect(check1_x, check1_y, check1b_x, check1b_y, check2_x, check2_y, check2b_x, check2b_y, &px, &py);
					double enx = px;
					double eny = py;
					if (num == 0) {
						first_x = px;
						first_y = py;
						if (mill_start == 0) {
							mill_move_in(first_x, first_y, depth, lasermode);
							mill_start = 1;
							last_x = first_x;
							last_y = first_y;
						}
						append_gcode("\n");
						mill_z(1, depth);
						if (overcut == 1 && ((myLINES[lnum1].type == TYPE_LINE && myLINES[lnum2].type == TYPE_LINE) || (myLINES[lnum1].type == TYPE_ELLIPSE && myLINES[lnum2].type == TYPE_ELLIPSE))) {
							double adx = myLINES[lnum2].x1 - px;
							double ady = myLINES[lnum2].y1 - py;
							double aalpha = toDeg(atan(ady / adx));
							if (adx < 0 && ady >= 0) {
								aalpha = aalpha + 180;
							} else if (adx < 0 && ady < 0) {
								aalpha = aalpha - 180;
							}
							double len = sqrt(adx * adx + ady * ady);
							add_angle_offset(&enx, &eny, len - tool_offset, aalpha);
							mill_xy(1, enx, eny, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
							mill_xy(1, px, py, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
							last_x = px;
							last_y = py;
						}
					} else {
						if (overcut == 1 && ((myLINES[lnum1].type == TYPE_LINE && myLINES[lnum2].type == TYPE_LINE) || (myLINES[lnum1].type == TYPE_ELLIPSE && myLINES[lnum2].type == TYPE_ELLIPSE))) {
							double adx = myLINES[lnum1].x2 - px;
							double ady = myLINES[lnum1].y2 - py;
							double aalpha = toDeg(atan(ady / adx));
							if (adx < 0 && ady >= 0) {
								aalpha = aalpha + 180;
							} else if (adx < 0 && ady < 0) {
								aalpha = aalpha - 180;
							}
							double len = sqrt(adx * adx + ady * ady);
							add_angle_offset(&enx, &eny, len - tool_offset, aalpha);
						}
						if (myLINES[lnum1].type == TYPE_ARC || myLINES[lnum1].type == TYPE_CIRCLE) {
							if (myOBJECTS[object_num].climb == 0) {
								if (myLINES[lnum1].opt < 0) {
									mill_xy(2, px, py, (myLINES[lnum1].opt + tool_offset) * -1, PARAMETER[P_M_FEEDRATE].vint, "");
								} else {
									mill_xy(3, px, py, (myLINES[lnum1].opt + tool_offset), PARAMETER[P_M_FEEDRATE].vint, "");
								}
							} else {
								if (myLINES[lnum1].opt < 0) {
									mill_xy(2, px, py, (myLINES[lnum1].opt - tool_offset) * -1, PARAMETER[P_M_FEEDRATE].vint, "");
								} else {
									mill_xy(3, px, py, (myLINES[lnum1].opt - tool_offset), PARAMETER[P_M_FEEDRATE].vint, "");
								}
							}
						} else {
							mill_xy(1, px, py, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
							if (overcut == 1 && ((myLINES[lnum1].type == TYPE_LINE && myLINES[lnum2].type == TYPE_LINE) || (myLINES[lnum1].type == TYPE_ELLIPSE && myLINES[lnum2].type == TYPE_ELLIPSE))) {
								mill_xy(1, enx, eny, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
								mill_xy(1, px, py, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
							}
						}
						last_x = px;
						last_y = py;
					}
				}
			} else {
				if (num == 0) {
					first_x = myLINES[lnum2].x1;
					first_y = myLINES[lnum2].y1;
					mill_move_in(first_x, first_y, depth, lasermode);
					mill_start = 1;
					append_gcode("\n");
					mill_z(1, depth);
				}
				double alpha1 = line_angle2(lnum1);
				double alpha2 = line_angle2(lnum2);
				double alpha_diff = alpha2 - alpha1;
				if (myLINES[lnum2].type == TYPE_ARC || myLINES[lnum2].type == TYPE_CIRCLE) {
					if (myLINES[lnum2].opt < 0) {
						mill_xy(2, myLINES[lnum2].x2, myLINES[lnum2].y2, myLINES[lnum2].opt * -1, PARAMETER[P_M_FEEDRATE].vint, "");
					} else {
						mill_xy(3, myLINES[lnum2].x2, myLINES[lnum2].y2, myLINES[lnum2].opt, PARAMETER[P_M_FEEDRATE].vint, "");
					}
				} else {
					if (PARAMETER[P_M_KNIFEMODE].vint == 1) {
						if (alpha_diff > 180.0) {
							alpha_diff -= 360.0;
						}
						if (alpha_diff < -180.0) {
							alpha_diff += 360.0;
						}
						if (alpha_diff > PARAMETER[P_H_KNIFEMAXANGLE].vdouble || alpha_diff < -PARAMETER[P_H_KNIFEMAXANGLE].vdouble) {
							mill_z(0, PARAMETER[P_CUT_SAVE].vdouble);
							sprintf(cline, "  (TAN: %f)\n", alpha2);
							append_gcode(cline);
							mill_z(1, depth);
						} else {
							sprintf(cline, "  (TAN: %f)\n", alpha2);
							append_gcode(cline);
						}
					}
					mill_xy(1, myLINES[lnum2].x2, myLINES[lnum2].y2, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
				}
				last_x = myLINES[lnum2].x2;
				last_y = myLINES[lnum2].y2;
			}
			last_lnum = lnum2;
		}
	}
	if (myOBJECTS[object_num].closed == 1) {
		if (myLINES[last].type == TYPE_ARC || myLINES[last].type == TYPE_CIRCLE) {
			if (myOBJECTS[object_num].climb == 0) {
				if (myLINES[last].opt < 0) {
					mill_xy(2, first_x, first_y, (myLINES[last].opt + tool_offset) * -1, PARAMETER[P_M_FEEDRATE].vint, "");
				} else {
					mill_xy(3, first_x, first_y, (myLINES[last].opt + tool_offset), PARAMETER[P_M_FEEDRATE].vint, "");
				}
			} else {
				if (myLINES[last].opt < 0) {
					mill_xy(2, first_x, first_y, (myLINES[last].opt - tool_offset) * -1, PARAMETER[P_M_FEEDRATE].vint, "");
				} else {
					mill_xy(3, first_x, first_y, (myLINES[last].opt - tool_offset), PARAMETER[P_M_FEEDRATE].vint, "");
				}
			}
		} else {
			mill_xy(1, first_x, first_y, 0.0, PARAMETER[P_M_FEEDRATE].vint, "");
		}
		last_x = first_x;
		last_y = first_y;
	}
	*next_x = last_x;
	*next_y = last_y;
	append_gcode("\n");
	if (error > 0) {
		return;
	}
}


void object_draw_offset (FILE *fd_out, int object_num, double *next_x, double *next_y) {
	double depth = 0.0;
	double tool_offset = 0.0;
	int overcut = 0;
	int lasermode = 0;
	int tangencialmode = 0;
	int offset = 0;
	double mill_depth_real = 0.0;
	if (PARAMETER[P_M_LASERMODE].vint == 1) {
		lasermode = 1;
		PARAMETER[P_M_KNIFEMODE].vint = 0;
	}
	if (PARAMETER[P_M_KNIFEMODE].vint == 1) {
		tangencialmode = 1;
	}

	if (strncmp(myOBJECTS[object_num].layer, "knife", 5) == 0) {
		tangencialmode = 1;
	}

	offset = myOBJECTS[object_num].offset;
	mill_depth_real = myOBJECTS[object_num].depth;
	overcut = myOBJECTS[object_num].overcut;
	lasermode = myOBJECTS[object_num].laser;

	tool_offset = PARAMETER[P_TOOL_DIAMETER].vdouble / 2.0;
	if (myOBJECTS[object_num].use == 0) {
		return;
	}

#ifdef USE_POSTCAM
	postcam_comment("--------------------------------------------------");
	sprintf(cline, "Object: #%i", object_num);
	postcam_var_push_string("partName", cline);
	postcam_comment(cline);
	sprintf(cline, "Layer: %s", myOBJECTS[object_num].layer);
	postcam_comment(cline);
	sprintf(cline, "Overcut: %i",  overcut);
	postcam_comment(cline);
	if (tangencialmode == 1) {
		sprintf(cline, "Tangencial-Mode: On");
	} else if (lasermode == 1) {
		sprintf(cline, "Laser-Mode: On");
	} else { 
		sprintf(cline, "Depth: %f", mill_depth_real);
	}
	postcam_comment(cline);
	if (offset == 0) {
		sprintf(cline, "Offset: None");
	} else if (offset == 1) {
		sprintf(cline, "Offset: Inside");
	} else {
		sprintf(cline, "Offset: Outside");
	}
	postcam_comment(cline);
	postcam_comment("--------------------------------------------------");
	postcam_call_function("OnNewPart");
#endif

	mill_start = 0;

	// offset for each depth-step
	double new_depth = 0.0;
	if (lasermode == 1 || tangencialmode == 1) {
		object_draw_offset_depth(fd_out, object_num, 0.0, next_x, next_y, tool_offset, overcut, lasermode, offset);
	} else {
		for (depth = PARAMETER[P_M_Z_STEP].vdouble; depth > mill_depth_real + PARAMETER[P_M_Z_STEP].vdouble; depth += PARAMETER[P_M_Z_STEP].vdouble) {
			if (depth < mill_depth_real) {
				new_depth = mill_depth_real;
			} else {
				new_depth = depth;
			}
			object_draw_offset_depth(fd_out, object_num, new_depth, next_x, next_y, tool_offset, overcut, lasermode, offset);
		}
	}
	mill_move_out(lasermode);
}


int find_next_line (int object_num, int first, int num, int dir, int depth) {
	int fnum = 0;
	int num4 = 0;
	int num5 = 0;
	double px = 0;
	double py = 0;
	int ret = 0;
	if (dir == 0) {
		px = myLINES[num].x1;
		py = myLINES[num].y1;
	} else {
		px = myLINES[num].x2;
		py = myLINES[num].y2;
	}
//	for (num4 = 0; num4 < depth; num4++) {
//		printf(" ");
//	}
	for (num5 = 0; num5 < object_last; num5++) {
		if (myOBJECTS[num5].line[0] == 0) {
			break;
		}
		for (num4 = 0; num4 < line_last; num4++) {
			if (myOBJECTS[num5].line[num4] == num) {
//				printf("##LINE %i in OBJECT %i / %i\n", num, num5, num4);
				return 2;
			}
		}
	}
	for (num4 = 0; num4 < line_last; num4++) {
		if (myOBJECTS[object_num].line[num4] == 0) {
//			printf("##ADD LINE %i to OBJECT %i / %i\n", num, object_num, num4);
			myOBJECTS[object_num].line[num4] = num;
			strcpy(myOBJECTS[object_num].layer, myLINES[num].layer);
			break;
		}
	}
	int num2 = 0;

	fnum = 0;
	for (num2 = 1; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1 && num != num2 && strcmp(myLINES[num2].layer, myLINES[num].layer) == 0) {
			if (px >= myLINES[num2].x1 - FUZZY && px <= myLINES[num2].x1 + FUZZY && py >= myLINES[num2].y1 - FUZZY && py <= myLINES[num2].y1 + FUZZY) {
//				printf("###### %i NEXT LINE: %f,%f -> %f,%f START\n", depth, myLINES[num2].x1, myLINES[num2].y1, myLINES[num2].x2, myLINES[num2].y2);
				fnum++;
			} else if (px >= myLINES[num2].x2 - FUZZY && px <= myLINES[num2].x2 + FUZZY && py >= myLINES[num2].y2 - FUZZY && py <= myLINES[num2].y2 + FUZZY) {
//				printf("###### %i NEXT LINE: %f,%f -> %f,%f END\n", depth, myLINES[num2].x1, myLINES[num2].y1, myLINES[num2].x2, myLINES[num2].y2);
				fnum++;
			}
		}
	}
	for (num2 = 1; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1 && num != num2 && strcmp(myLINES[num2].layer, myLINES[num].layer) == 0) {
			if (px >= myLINES[num2].x1 - FUZZY && px <= myLINES[num2].x1 + FUZZY && py >= myLINES[num2].y1 - FUZZY && py <= myLINES[num2].y1 + FUZZY) {
//				printf("###### %i NEXT LINE: %f,%f -> %f,%f START\n", depth, myLINES[num2].x1, myLINES[num2].y1, myLINES[num2].x2, myLINES[num2].y2);
				if (num2 != first) {
					ret = find_next_line(object_num, first, num2, 1, depth + 1);
					if (ret == 1) {
						return 1;
					}
				} else {
//					printf("###### OBJECT CLOSED\n");
					return 1;
				}
			} else if (px >= myLINES[num2].x2 - FUZZY && px <= myLINES[num2].x2 + FUZZY && py >= myLINES[num2].y2 - FUZZY && py <= myLINES[num2].y2 + FUZZY) {
				line_invert(num2);
//				printf("###### %i NEXT LINE: %f,%f -> %f,%f END\n", depth, myLINES[num2].x1, myLINES[num2].y1, myLINES[num2].x2, myLINES[num2].y2);
				if (num2 != first) {
					ret = find_next_line(object_num, first, num2, 1, depth + 1);
					if (ret == 1) {
						return 1;
					}
				} else {
//					printf("###### OBJECT CLOSED\n");
					return 1;
				}
			}
		}
	}
	return ret;
}

int line_open_check (int num) {
	int ret = 0;
	int dir = 0;
	int num2 = 0;
	int onum = 0;
	double px = 0.0;
	double py = 0.0;
	for (onum = 0; onum < object_last; onum++) {
		if (myOBJECTS[onum].line[0] == 0) {
			break;
		}
		for (num2 = 0; num2 < line_last; num2++) {
			if (myOBJECTS[onum].line[num2] == num) {
//				printf("##LINE %i in OBJECT %i / %i\n", num, onum, num2);
				return 0;
			}
		}
	}
	px = myLINES[num].x1;
	py = myLINES[num].y1;
	for (num2 = 1; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1 && num != num2 && strcmp(myLINES[num2].layer, myLINES[num].layer) == 0) {
			if (px >= myLINES[num2].x1 - FUZZY && px <= myLINES[num2].x1 + FUZZY && py >= myLINES[num2].y1 - FUZZY && py <= myLINES[num2].y1 + FUZZY) {
				ret++;
				dir = 1;
				break;
			} else if (px >= myLINES[num2].x2 - FUZZY && px <= myLINES[num2].x2 + FUZZY && py >= myLINES[num2].y2 - FUZZY && py <= myLINES[num2].y2 + FUZZY) {
				ret++;
				dir = 1;
				break;
			}
		}
	}
	px = myLINES[num].x2;
	py = myLINES[num].y2;
	for (num2 = 1; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1 && num != num2 && strcmp(myLINES[num2].layer, myLINES[num].layer) == 0) {
			if (px >= myLINES[num2].x1 - FUZZY && px <= myLINES[num2].x1 + FUZZY && py >= myLINES[num2].y1 - FUZZY && py <= myLINES[num2].y1 + FUZZY) {
				ret++;
				dir = 2;
				break;
			} else if (px >= myLINES[num2].x2 - FUZZY && px <= myLINES[num2].x2 + FUZZY && py >= myLINES[num2].y2 - FUZZY && py <= myLINES[num2].y2 + FUZZY) {
				ret++;
				dir = 2;
				break;
			}
		}
	}
	if (ret == 1) {
		return dir;
	} else if (ret == 0) {
		return 3;
	}
	return 0;
}

void SetQuaternionFromAxisAngle (const float *axis, float angle, float *quat) {
	float sina2, norm;
	sina2 = (float)sin(0.5f * angle);
	norm = (float)sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
	quat[0] = sina2 * axis[0] / norm;
	quat[1] = sina2 * axis[1] / norm;
	quat[2] = sina2 * axis[2] / norm;
	quat[3] = (float)cos(0.5f * angle);
}

void ConvertQuaternionToMatrix (const float *quat, float *mat) {
	float yy2 = 2.0f * quat[1] * quat[1];
	float xy2 = 2.0f * quat[0] * quat[1];
	float xz2 = 2.0f * quat[0] * quat[2];
	float yz2 = 2.0f * quat[1] * quat[2];
	float zz2 = 2.0f * quat[2] * quat[2];
	float wz2 = 2.0f * quat[3] * quat[2];
	float wy2 = 2.0f * quat[3] * quat[1];
	float wx2 = 2.0f * quat[3] * quat[0];
	float xx2 = 2.0f * quat[0] * quat[0];
	mat[0*4+0] = - yy2 - zz2 + 1.0f;
	mat[0*4+1] = xy2 + wz2;
	mat[0*4+2] = xz2 - wy2;
	mat[0*4+3] = 0;
	mat[1*4+0] = xy2 - wz2;
	mat[1*4+1] = - xx2 - zz2 + 1.0f;
	mat[1*4+2] = yz2 + wx2;
	mat[1*4+3] = 0;
	mat[2*4+0] = xz2 + wy2;
	mat[2*4+1] = yz2 - wx2;
	mat[2*4+2] = - xx2 - yy2 + 1.0f;
	mat[2*4+3] = 0;
	mat[3*4+0] = mat[3*4+1] = mat[3*4+2] = 0;
	mat[3*4+3] = 1;
}

void MultiplyQuaternions (const float *q1, const float *q2, float *qout) {
	float qr[4];
	qr[0] = q1[3]*q2[0] + q1[0]*q2[3] + q1[1]*q2[2] - q1[2]*q2[1];
	qr[1] = q1[3]*q2[1] + q1[1]*q2[3] + q1[2]*q2[0] - q1[0]*q2[2];
	qr[2] = q1[3]*q2[2] + q1[2]*q2[3] + q1[0]*q2[1] - q1[1]*q2[0];
	qr[3]  = q1[3]*q2[3] - (q1[0]*q2[0] + q1[1]*q2[1] + q1[2]*q2[2]);
	qout[0] = qr[0]; qout[1] = qr[1]; qout[2] = qr[2]; qout[3] = qr[3];
}

void onExit (void) {
}

void init_objects (void) {
	int num2 = 0;
	int num5b = 0;
	int object_num = 0;

	/* init objects */
	if (myOBJECTS != NULL) {
		free(myOBJECTS);
		myOBJECTS = NULL;
	}
	myOBJECTS = malloc(sizeof(_OBJECT) * (line_last + 1));
	for (object_num = 0; object_num < line_last; object_num++) {
		myOBJECTS[object_num].use = 1;
		myOBJECTS[object_num].closed = 0;
		myOBJECTS[object_num].climb = 0;
		myOBJECTS[object_num].force = 0;
		myOBJECTS[object_num].offset = 0;
		myOBJECTS[object_num].overcut = 0;
		myOBJECTS[object_num].pocket = 0;
		myOBJECTS[object_num].laser = 0;
		myOBJECTS[object_num].visited = 0;
		myOBJECTS[object_num].depth = 0.0;
		myOBJECTS[object_num].layer[0] = 0;
		myOBJECTS[object_num].min_x = 999999.0;
		myOBJECTS[object_num].min_y = 999999.0;
		myOBJECTS[object_num].max_x = -999999.0;
		myOBJECTS[object_num].max_y = -999999.0;
		for (num2 = 0; num2 < line_last; num2++) {
			myOBJECTS[object_num].line[num2] = 0;
		}
	}

	/* first find objects on open lines */
	object_num = 0;
	for (num2 = 1; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1) {
			int ends = line_open_check(num2);
			if (ends == 1) {
				line_invert(num2);
			}
			if (ends > 0) {
				find_next_line(object_num, num2, num2, 1, 0);
				myOBJECTS[object_num].closed = 0;
				object_num++;
				object_last = object_num + 2;
			}
		}
	}

	/* find objects and check if open or close */
	for (num2 = 1; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1) {
			int ret = find_next_line(object_num, num2, num2, 1, 0);
			if (myLINES[num2].type == TYPE_MTEXT) {
				myOBJECTS[object_num].closed = 0;
				object_num++;
			} else if (ret == 1) {
				myOBJECTS[object_num].closed = 1;
				object_num++;
			} else if (ret == 0) {
				myOBJECTS[object_num].closed = 0;
				object_num++;
			}
			object_last = object_num + 2;
		}
	}

	/* check if object inside or outside */
	for (num5b = 0; num5b < object_last; num5b++) {
		int flag = 0;
/*
		int lnum = myOBJECTS[num5b].line[0];
		if (myLINES[lnum].used == 1) {
			int pipret = 0;
			double testx = myLINES[lnum].x1;
			double testy = myLINES[lnum].y1;
			pipret = point_in_object(-1, num5b, testx, testy);
			if (pipret != 0) {
				flag = 1;
			}
			pipret = 0;
			testx = myLINES[lnum].x2;
			testy = myLINES[lnum].y2;
			pipret = point_in_object(-1, num5b, testx, testy);
			if (pipret != 0) {
				flag = 1;
			}
		}
*/
		int num4b = 0;
		for (num4b = 0; num4b < line_last; num4b++) {
			if (myOBJECTS[num5b].line[num4b] != 0) {
				int lnum = myOBJECTS[num5b].line[num4b];
				if (myLINES[lnum].used == 1) {
					int pipret = 0;
					double testx = myLINES[lnum].x1;
					double testy = myLINES[lnum].y1;
					pipret = point_in_object(-1, num5b, testx, testy);
					if (pipret != 0) {
						flag = 1;
					}
					pipret = 0;
					testx = myLINES[lnum].x2;
					testy = myLINES[lnum].y2;
					pipret = point_in_object(-1, num5b, testx, testy);
					if (pipret != 0) {
						flag = 1;
					}
				}
			}
		}

		if (flag > 0) {
			myOBJECTS[num5b].inside = 1;
		} else if (myOBJECTS[num5b].line[0] != 0) {
			myOBJECTS[num5b].inside = 0;
		}
	}

	for (object_num = 0; object_num < line_last; object_num++) {
		if (myOBJECTS[object_num].line[0] != 0) {
			if (strncmp(myOBJECTS[object_num].layer, "offset-inside", 13) == 0) {
				if (myOBJECTS[object_num].inside == 0) {
					redir_object(object_num);
					myOBJECTS[object_num].inside = 1;
				}
			} else if (strncmp(myOBJECTS[object_num].layer, "offset-outside", 14) == 0) {
				if (myOBJECTS[object_num].inside == 1) {
					redir_object(object_num);
					myOBJECTS[object_num].inside = 0;
				}
			}
		}
	}

	// object-boundingbox
	for (num5b = 0; num5b < object_last; num5b++) {
		if (myLINES[myOBJECTS[num5b].line[0]].type == TYPE_CIRCLE) {
			int lnum = myOBJECTS[num5b].line[0];
			if (myOBJECTS[num5b].min_x > myLINES[lnum].cx - myLINES[lnum].opt) {
				myOBJECTS[num5b].min_x = myLINES[lnum].cx - myLINES[lnum].opt;
			}
			if (myOBJECTS[num5b].min_y > myLINES[lnum].cy - myLINES[lnum].opt) {
				myOBJECTS[num5b].min_y = myLINES[lnum].cy - myLINES[lnum].opt;
			}
			if (myOBJECTS[num5b].max_x < myLINES[lnum].cx + myLINES[lnum].opt) {
				myOBJECTS[num5b].max_x = myLINES[lnum].cx + myLINES[lnum].opt;
			}
			if (myOBJECTS[num5b].max_y < myLINES[lnum].cy + myLINES[lnum].opt) {
				myOBJECTS[num5b].max_y = myLINES[lnum].cy + myLINES[lnum].opt;
			}


		} else {
			int num4b = 0;
			for (num4b = 0; num4b < line_last; num4b++) {
				int lnum = myOBJECTS[num5b].line[num4b];
				if (myLINES[lnum].used == 1) {
					if (myOBJECTS[num5b].min_x > myLINES[lnum].x1) {
						myOBJECTS[num5b].min_x = myLINES[lnum].x1;
					}
					if (myOBJECTS[num5b].min_y > myLINES[lnum].y1) {
						myOBJECTS[num5b].min_y = myLINES[lnum].y1;
					}
					if (myOBJECTS[num5b].min_x > myLINES[lnum].x2) {
						myOBJECTS[num5b].min_x = myLINES[lnum].x2;
					}
					if (myOBJECTS[num5b].min_y > myLINES[lnum].y2) {
						myOBJECTS[num5b].min_y = myLINES[lnum].y2;
					}
					if (myOBJECTS[num5b].max_x < myLINES[lnum].x1) {
						myOBJECTS[num5b].max_x = myLINES[lnum].x1;
					}
					if (myOBJECTS[num5b].max_y < myLINES[lnum].y1) {
						myOBJECTS[num5b].max_y = myLINES[lnum].y1;
					}
					if (myOBJECTS[num5b].max_x < myLINES[lnum].x2) {
						myOBJECTS[num5b].max_x = myLINES[lnum].x2;
					}
					if (myOBJECTS[num5b].max_y < myLINES[lnum].y2) {
						myOBJECTS[num5b].max_y = myLINES[lnum].y2;
					}
				}
			}
		}
	}

	PARAMETER[P_O_SELECT].vint = -1;
	gtk_list_store_clear(ListStore[P_O_SELECT]);
	for (object_num = 0; object_num < line_last; object_num++) {
		if (myOBJECTS[object_num].line[0] != 0) {
			char tmp_str[128];	
			sprintf(tmp_str, "Object: #%i (%s)", object_num, myOBJECTS[object_num].layer);
			gtk_list_store_insert_with_values(ListStore[P_O_SELECT], NULL, object_num, 0, NULL, 1, tmp_str, -1);
		}
	}
	update_post = 1;
}

void draw_helplines (void) {
	char tmp_str[128];
	if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
		GLUquadricObj *quadratic = gluNewQuadric();
		float radius = (PARAMETER[P_MAT_DIAMETER].vdouble / 2.0) + PARAMETER[P_M_DEPTH].vdouble;
		float radius2 = (PARAMETER[P_MAT_DIAMETER].vdouble / 2.0);
		glPushMatrix();
		glTranslatef(0.0, -radius2 - 10.0, 0.0);
		float lenX = size_x;
		float offXYZ = 10.0;
		float arrow_d = 1.0;
		float arrow_l = 6.0;
		glColor4f(0.0, 1.0, 0.0, 1.0);
		glBegin(GL_LINES);
		glVertex3f(0.0, -offXYZ, 0.0);
		glVertex3f(0.0, 0.0, 0.0);
		glEnd();
		glBegin(GL_LINES);
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3f(lenX, 0.0, 0.0);
		glEnd();
		glBegin(GL_LINES);
		glVertex3f(lenX, -offXYZ, 0.0);
		glVertex3f(lenX, 0.0, 0.0);
		glEnd();
		glPushMatrix();
		glTranslatef(lenX, -offXYZ, 0.0);
		glPushMatrix();
		glTranslatef(-lenX / 2.0, -arrow_d * 2.0 - 11.0, 0.0);
		sprintf(tmp_str, "%0.2fmm", lenX);
		output_text_gl_center(tmp_str, 0.0, 0.0, 0.0, 0.2);
		glPopMatrix();
		glRotatef(-90.0, 0.0, 1.0, 0.0);
		gluCylinder(quadratic, 0.0, (arrow_d * 3), arrow_l ,32, 1);
		glTranslatef(0.0, 0.0, arrow_l);
		gluCylinder(quadratic, arrow_d, arrow_d, lenX - arrow_l * 2.0 ,32, 1);
		glTranslatef(0.0, 0.0, lenX - arrow_l * 2.0);
		gluCylinder(quadratic, (arrow_d * 3), 0.0, arrow_l ,32, 1);
		glPopMatrix();
		glPopMatrix();

		glColor4f(0.2, 0.2, 0.2, 0.5);
		glPushMatrix();
		glRotatef(90.0, 0.0, 1.0, 0.0);
		gluCylinder(quadratic, radius, radius, size_x ,64, 1);
		glTranslatef(0.0, 0.0, -size_x);
		gluCylinder(quadratic, radius2, radius2, size_x ,64, 1);
		glTranslatef(0.0, 0.0, size_x * 2);
		gluCylinder(quadratic, radius2, radius2, size_x ,64, 1);
		glPopMatrix();

		return;
	}

	/* Scale-Arrow's */
	float gridXYZ = PARAMETER[P_V_HELP_GRID].vfloat * 10.0;
	float gridXYZmin = PARAMETER[P_V_HELP_GRID].vfloat;
	float lenY = size_y;
	float lenX = size_x;
	float lenZ = PARAMETER[P_M_DEPTH].vdouble * -1;
	float offXYZ = 10.0;
	float arrow_d = 1.0 * PARAMETER[P_V_HELP_ARROW].vfloat;
	float arrow_l = 6.0 * PARAMETER[P_V_HELP_ARROW].vfloat;
	GLUquadricObj *quadratic = gluNewQuadric();

	int pos_n = 0;
	glColor4f(1.0, 1.0, 1.0, 0.3);
	for (pos_n = 0; pos_n <= lenY; pos_n += gridXYZ) {
		glBegin(GL_LINES);
		glVertex3f(0.0, pos_n, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glVertex3f(lenX, pos_n, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glEnd();
	}
	for (pos_n = 0; pos_n <= lenX; pos_n += gridXYZ) {
		glBegin(GL_LINES);
		glVertex3f(pos_n, 0.0, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glVertex3f(pos_n, lenY, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glEnd();
	}
	glColor4f(1.0, 1.0, 1.0, 0.2);
	for (pos_n = 0; pos_n <= lenY; pos_n += gridXYZmin) {
		glBegin(GL_LINES);
		glVertex3f(0.0, pos_n, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glVertex3f(lenX, pos_n, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glEnd();
	}
	for (pos_n = 0; pos_n <= lenX; pos_n += gridXYZmin) {
		glBegin(GL_LINES);
		glVertex3f(pos_n, 0.0, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glVertex3f(pos_n, lenY, PARAMETER[P_M_DEPTH].vdouble - 0.1);
		glEnd();
	}

	glColor4f(1.0, 0.0, 0.0, 1.0);
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(-offXYZ, 0.0, 0.0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, lenY, 0.0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(0.0, lenY, 0.0);
	glVertex3f(-offXYZ, lenY, 0.0);
	glEnd();
	glPushMatrix();
	glTranslatef(0.0 - offXYZ, -0.0, 0.0);
	glPushMatrix();
	glTranslatef(arrow_d * 2.0 + 1.0, lenY / 2.0, 0.0);
	glRotatef(90.0, 0.0, 0.0, 1.0);
	sprintf(tmp_str, "%0.2fmm", lenY);
	glPushMatrix();
	glTranslatef(0.0, 4.0, 0.0);
	output_text_gl_center(tmp_str, 0.0, 0.0, 0.0, 0.2);
	glPopMatrix();
	glPopMatrix();
	glRotatef(-90.0, 1.0, 0.0, 0.0);
	gluCylinder(quadratic, 0.0, (arrow_d * 3), arrow_l ,32, 1);
	glTranslatef(0.0, 0.0, arrow_l);
	gluCylinder(quadratic, arrow_d, arrow_d, lenY - arrow_l * 2.0 ,32, 1);
	glTranslatef(0.0, 0.0, lenY - arrow_l * 2.0);
	gluCylinder(quadratic, (arrow_d * 3), 0.0, arrow_l ,32, 1);
	glPopMatrix();

	glColor4f(0.0, 1.0, 0.0, 1.0);
	glBegin(GL_LINES);
	glVertex3f(0.0, -offXYZ, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(lenX, 0.0, 0.0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(lenX, -offXYZ, 0.0);
	glVertex3f(lenX, 0.0, 0.0);
	glEnd();
	glPushMatrix();
	glTranslatef(lenX, -offXYZ, 0.0);
	glPushMatrix();
	glTranslatef(-lenX / 2.0, -arrow_d * 2.0 - 1.0, 0.0);
	sprintf(tmp_str, "%0.2fmm", lenX);
	glPushMatrix();
	glTranslatef(0.0, -4.0, 0.0);
	output_text_gl_center(tmp_str, 0.0, 0.0, 0.0, 0.2);
	glPopMatrix();
	glPopMatrix();
	glRotatef(-90.0, 0.0, 1.0, 0.0);
	gluCylinder(quadratic, 0.0, (arrow_d * 3), arrow_l ,32, 1);
	glTranslatef(0.0, 0.0, arrow_l);
	gluCylinder(quadratic, arrow_d, arrow_d, lenX - arrow_l * 2.0 ,32, 1);
	glTranslatef(0.0, 0.0, lenX - arrow_l * 2.0);
	gluCylinder(quadratic, (arrow_d * 3), 0.0, arrow_l ,32, 1);
	glPopMatrix();

	glColor4f(0.0, 0.0, 1.0, 1.0);
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(-offXYZ, -offXYZ, 0.0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, -lenZ);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, -lenZ);
	glVertex3f(-offXYZ, -offXYZ, -lenZ);
	glEnd();
	glPushMatrix();
	glTranslatef(-offXYZ, -offXYZ, -lenZ);
	glPushMatrix();
	glTranslatef(arrow_d * 2.0 - 1.0, -arrow_d * 2.0 - 1.0, lenZ / 2.0);
	glRotatef(90.0, 0.0, 1.0, 0.0);
	sprintf(tmp_str, "%0.2fmm", lenZ);
	glPushMatrix();
	glTranslatef(0.0, -4.0, 0.0);
	output_text_gl_center(tmp_str, 0.0, 0.0, 0.0, 0.2);
	glPopMatrix();
	glPopMatrix();
	glRotatef(-90.0, 0.0, 0.0, 1.0);
	gluCylinder(quadratic, 0.0, (arrow_d * 3), arrow_l ,32, 1);
	glTranslatef(0.0, 0.0, arrow_l);
	gluCylinder(quadratic, arrow_d, arrow_d, lenZ - arrow_l * 2.0 ,32, 1);
	glTranslatef(0.0, 0.0, lenZ - arrow_l * 2.0);
	gluCylinder(quadratic, (arrow_d * 3), 0.0, arrow_l ,32, 1);
	glPopMatrix();
}


void mainloop (void) {
	char tmp_str[1024];
	int object_num = 0;
	size_x = (max_x - min_x);
	size_y = (max_y - min_y);
	float scale = (4.0 / size_x);
	if (scale > (4.0 / size_y)) {
		scale = (4.0 / size_y);
	}

	/* get diameter from tooltable by number */
	if (PARAMETER[P_TOOL_SELECT].vint > 0) {
		PARAMETER[P_TOOL_NUM].vint = PARAMETER[P_TOOL_SELECT].vint;
		PARAMETER[P_TOOL_DIAMETER].vdouble = tooltbl_diameters[PARAMETER[P_TOOL_NUM].vint];
	}
	PARAMETER[P_TOOL_SPEED_MAX].vint = (int)(((float)material_vc[PARAMETER[P_MAT_SELECT].vint] * 1000.0) / (PI * (PARAMETER[P_TOOL_DIAMETER].vdouble)));
	if ((PARAMETER[P_TOOL_DIAMETER].vdouble) < 4.0) {
		PARAMETER[P_M_FEEDRATE_MAX].vint = (int)((float)PARAMETER[P_TOOL_SPEED].vint * material_fz[PARAMETER[P_MAT_SELECT].vint][0] * (float)PARAMETER[P_TOOL_W].vint);
	} else if ((PARAMETER[P_TOOL_DIAMETER].vdouble) < 8.0) {
		PARAMETER[P_M_FEEDRATE_MAX].vint = (int)((float)PARAMETER[P_TOOL_SPEED].vint * material_fz[PARAMETER[P_MAT_SELECT].vint][1] * (float)PARAMETER[P_TOOL_W].vint);
	} else if ((PARAMETER[P_TOOL_DIAMETER].vdouble) < 12.0) {
		PARAMETER[P_M_FEEDRATE_MAX].vint = (int)((float)PARAMETER[P_TOOL_SPEED].vint * material_fz[PARAMETER[P_MAT_SELECT].vint][2] * (float)PARAMETER[P_TOOL_W].vint);
	}

	if (update_post == 1) {
		glDeleteLists(1, 1);
		glNewList(1, GL_COMPILE);

		if (PARAMETER[P_V_HELPLINES].vint == 1) {
			if (PARAMETER[P_M_ROTARYMODE].vint == 0) {
				draw_helplines();
			}
		}

		// init output
		mill_start_all = 0;
		tool_last = -1;
		mill_distance_xy = 0.0;
		mill_distance_z = 0.0;
		move_distance_xy = 0.0;
		move_distance_z = 0.0;
		mill_last_x = 0.0;
		mill_last_y = 0.0;
		mill_last_z = PARAMETER[P_CUT_SAVE].vdouble;
		if (gcode_buffer != NULL) {
			free(gcode_buffer);
			gcode_buffer = NULL;
		}
#ifdef USE_POSTCAM
		if (postcam_plugin != PARAMETER[P_H_POST].vint) {
			postcam_exit_lua();
			strcpy(output_extension, "ngc");
			strcpy(output_info, "");
			postcam_init_lua(postcam_plugins[PARAMETER[P_H_POST].vint]);
			postcam_plugin = PARAMETER[P_H_POST].vint;
			gtk_label_set_text(GTK_LABEL(OutputInfoLabel), output_info);
			sprintf(tmp_str, "Output (%s)", output_extension);
			gtk_label_set_text(GTK_LABEL(gCodeViewLabel), tmp_str);
			postcam_load_source(postcam_plugins[PARAMETER[P_H_POST].vint]);
		}
		postcam_var_push_string("fileName", PARAMETER[P_V_DXF].vstr);
		postcam_var_push_string("postName", postcam_plugins[PARAMETER[P_H_POST].vint]);
		postcam_var_push_string("date", "---");
		postcam_var_push_double("metric", 1.0);
		postcam_var_push_int("feedRate", PARAMETER[P_M_PLUNGE_SPEED].vint);
		postcam_var_push_int("spindleSpeed", PARAMETER[P_TOOL_SPEED].vint);
		postcam_var_push_double("currentX", _X(0.0));
		postcam_var_push_double("currentY", _Y(0.0));
		postcam_var_push_double("currentZ", _Z(0.0));
		postcam_var_push_double("endX", _X(0.0));
		postcam_var_push_double("endY", _Y(0.0));
		postcam_var_push_double("endZ", _Z(0.0));
		postcam_var_push_double("toolOffset", 0.0);
		postcam_var_push_int("tool", -1);
		postcam_var_push_int("lastinst", 0);
	//	SetupShowGcode(fd_out);
		postcam_call_function("OnInit");
		if (PARAMETER[P_M_ROTARYMODE].vint == 1) {
			if (PARAMETER[P_H_ROTARYAXIS].vint == 1) {
				postcam_var_push_string("axisX", "B");
			} else {
				postcam_var_push_string("axisX", "X");
			}
			if (PARAMETER[P_H_ROTARYAXIS].vint == 0) {
				postcam_var_push_string("axisY", "A");
			} else {
				postcam_var_push_string("axisY", "Y");
			}
		} else {
			postcam_var_push_string("axisX", "X");
			postcam_var_push_string("axisY", "Y");
		}
		postcam_var_push_string("axisZ", "Z");
#else
		append_gcode("G21 (Metric)\n");
		append_gcode("G40 (No Offsets)\n");
		append_gcode("G90 (Absolute-Mode)\n");
		sprintf(cline, "F%i\n", PARAMETER[P_M_FEEDRATE].vint);
		append_gcode(cline);
#endif

		// update Object-Data
		for (object_num = 0; object_num < object_last; object_num++) {
			if (myOBJECTS[object_num].force == 0) {
				myOBJECTS[object_num].depth = PARAMETER[P_M_DEPTH].vdouble;
				myOBJECTS[object_num].overcut = PARAMETER[P_M_OVERCUT].vint;
				myOBJECTS[object_num].laser = PARAMETER[P_M_LASERMODE].vint;
				myOBJECTS[object_num].climb = PARAMETER[P_M_CLIMB].vint;
				if (PARAMETER[P_M_LASERMODE].vint == 1) {
					myOBJECTS[object_num].laser = 1;
				}
				if (strncmp(myOBJECTS[object_num].layer, "depth-", 6) == 0) {
					myOBJECTS[object_num].depth = atof(myOBJECTS[object_num].layer + 5);
				}
				if (strncmp(myOBJECTS[object_num].layer, "laser", 5) == 0) {
					myOBJECTS[object_num].laser = 1;
				}
				if (myOBJECTS[object_num].closed == 1) {
					if (myOBJECTS[object_num].inside == 1) {
						myOBJECTS[object_num].offset = 1; // INSIDE
					} else {
						myOBJECTS[object_num].offset = 2; // OUTSIDE
					}
				} else {
					myOBJECTS[object_num].offset = 0; // NONE
				}
			}
		}

		/* 'shortest' path / first inside than outside objects */ 
		double last_x = 0.0;
		double last_y = 0.0;
		double next_x = 0.0;
		double next_y = 0.0;
		for (object_num = 0; object_num < object_last; object_num++) {
			myOBJECTS[object_num].visited = 0;
		}

		/* inside and open objects */
		for (object_num = 0; object_num < object_last; object_num++) {
			double shortest_len = 9999999.0;
			int shortest_object = -1;
			int shortest_line = -1;
			int flag = 0;
			int object_num2 = 0;
			for (object_num2 = 0; object_num2 < object_last; object_num2++) {
				int nnum = 0;
				if (myLINES[myOBJECTS[object_num2].line[nnum]].type == TYPE_CIRCLE) {
					if (myOBJECTS[object_num2].line[nnum] != 0 && ((myOBJECTS[object_num2].force == 1 && myOBJECTS[object_num2].offset == 1) || myOBJECTS[object_num2].inside == 1) && myOBJECTS[object_num2].visited == 0) {
						int lnum2 = myOBJECTS[object_num2].line[nnum];
						double len = get_len(last_x, last_y, myLINES[lnum2].cx - myLINES[lnum2].opt, myLINES[lnum2].cy);
						if (len < shortest_len) {
							shortest_len = len;
							shortest_object = object_num2;
							shortest_line = nnum;
							flag = 1;
						}
					}
				} else {
					for (nnum = 0; nnum < line_last; nnum++) {
						if (myOBJECTS[object_num2].line[nnum] != 0 && ((myOBJECTS[object_num2].force == 1 && myOBJECTS[object_num2].offset == 1) || myOBJECTS[object_num2].inside == 1) && myOBJECTS[object_num2].visited == 0) {
							int lnum2 = myOBJECTS[object_num2].line[nnum];
							double len = get_len(last_x, last_y, myLINES[lnum2].x1, myLINES[lnum2].y1);
							if (len < shortest_len) {
								shortest_len = len;
								shortest_object = object_num2;
								shortest_line = nnum;
								flag = 1;
							}
						}
					}
				}
				nnum = 0;
				if (myOBJECTS[object_num2].line[nnum] != 0 && myOBJECTS[object_num2].closed == 0 && myOBJECTS[object_num2].visited == 0) {
					int lnum2 = myOBJECTS[object_num2].line[nnum];
					double len = get_len(last_x, last_y, myLINES[lnum2].x1, myLINES[lnum2].y1);
					if (len < shortest_len) {
						shortest_len = len;
						shortest_object = object_num2;
						shortest_line = nnum;
						flag = 1;
					}
				}
				nnum = object_line_last(object_num2);
				if (myOBJECTS[object_num2].line[nnum] != 0 && myOBJECTS[object_num2].closed == 0 && myOBJECTS[object_num2].visited == 0) {
					int lnum2 = myOBJECTS[object_num2].line[nnum];
					double len = get_len(last_x, last_y, myLINES[lnum2].x2, myLINES[lnum2].y2);
					if (len < shortest_len) {
						shortest_len = len;
						shortest_object = object_num2;
						shortest_line = nnum;
						flag = 2;
					}
				}
			}
			if (flag > 0) {
				myOBJECTS[shortest_object].visited = 1;
				if (flag > 1) {
					redir_object(shortest_object);
				}
				if (myOBJECTS[shortest_object].closed == 1 && myLINES[myOBJECTS[shortest_object].line[0]].type != TYPE_CIRCLE) {
					resort_object(shortest_object, shortest_line);
					object_optimize_dir(shortest_object);
				}
				object_draw_offset(fd_out, shortest_object, &next_x, &next_y);
				object_draw(fd_out, shortest_object);
				last_x = next_x;
				last_y = next_y;
	
			} else {
				break;
			}
		}

		/* outside objects */
		for (object_num = 0; object_num < object_last; object_num++) {
			double shortest_len = 9999999.0;
			int shortest_object = -1;
			int shortest_line = -1;
			int flag = 0;
			int object_num2 = 0;
			for (object_num2 = 0; object_num2 < object_last; object_num2++) {
				int nnum = 0;
				if (myLINES[myOBJECTS[object_num2].line[nnum]].type == TYPE_CIRCLE) {
					if (myOBJECTS[object_num2].line[nnum] != 0 && (((myOBJECTS[object_num2].force == 1 && myOBJECTS[object_num2].offset == 2) || myOBJECTS[object_num2].inside == 0) && myOBJECTS[object_num2].closed == 1) && myOBJECTS[object_num2].visited == 0) {
						int lnum2 = myOBJECTS[object_num2].line[nnum];
						double len = get_len(last_x, last_y, myLINES[lnum2].cx - myLINES[lnum2].opt, myLINES[lnum2].cy);
						if (len < shortest_len) {
							shortest_len = len;
							shortest_object = object_num2;
							shortest_line = nnum;
							flag = 1;
						}
					}
				} else {
					for (nnum = 0; nnum < line_last; nnum++) {
						if (myOBJECTS[object_num2].line[nnum] != 0 && (((myOBJECTS[object_num2].force == 1 && myOBJECTS[object_num2].offset == 2) || myOBJECTS[object_num2].inside == 0) && myOBJECTS[object_num2].closed == 1) && myOBJECTS[object_num2].visited == 0) {
							int lnum2 = myOBJECTS[object_num2].line[nnum];
							double len = get_len(last_x, last_y, myLINES[lnum2].x1, myLINES[lnum2].y1);
							if (len < shortest_len) {
								shortest_len = len;
								shortest_object = object_num2;
								shortest_line = nnum;
								flag = 1;
							}
						}
					}
				}
			}
			if (flag == 1) {
				myOBJECTS[shortest_object].visited = 1;
				if (myLINES[myOBJECTS[shortest_object].line[0]].type != TYPE_CIRCLE) {
					resort_object(shortest_object, shortest_line);
					object_optimize_dir(shortest_object);
				}
				if (myLINES[myOBJECTS[shortest_object].line[0]].type == TYPE_MTEXT) {
				} else {
					object_draw_offset(fd_out, shortest_object, &next_x, &next_y);
					object_draw(fd_out, shortest_object);
				}
				last_x = next_x;
				last_y = next_y;
			} else {
				break;
			}
		}

	/* exit output */
#ifdef USE_POSTCAM
		postcam_call_function("OnSpindleOff");
		postcam_call_function("OnFinish");
		gtk_label_set_text(GTK_LABEL(OutputErrorLabel), output_error);
#else
		append_gcode("M05 (Spindle-/Laser-Off)\n");
		append_gcode("M02 (Programm-End)\n");
#endif

		// update GUI
		GtkTextIter startLua, endLua;
		GtkTextBuffer *bufferLua;
		bufferLua = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCodeViewLua));
		gtk_text_buffer_get_bounds(bufferLua, &startLua, &endLua);

		update_post = 0;
		GtkTextIter start, end;
		GtkTextBuffer *buffer;
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCodeView));
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		char *gcode_check = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
		if (gcode_check != NULL) {
			if (gcode_buffer != NULL) {
				if (strcmp(gcode_check, gcode_buffer) != 0) {
					gtk_text_buffer_set_text(buffer, gcode_buffer, -1);
				}
			} else {
				gtk_text_buffer_set_text(buffer, "", -1);
			}
			free(gcode_check);
		} else {
			gtk_text_buffer_set_text(buffer, "", -1);
		}
		double milltime = mill_distance_xy / PARAMETER[P_M_FEEDRATE].vint;
		milltime += mill_distance_z / PARAMETER[P_M_PLUNGE_SPEED].vint;
		milltime += (move_distance_xy + move_distance_z) / PARAMETER[P_H_FEEDRATE_FAST].vint;
		sprintf(tmp_str, "Distance: Mill-XY=%0.2fmm/Z=%0.2fmm / Move-XY=%0.2fmm/Z=%0.2fmm / Time>%0.1fmin", mill_distance_xy, mill_distance_z, move_distance_xy, move_distance_z, milltime);
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), tmp_str), tmp_str);
		sprintf(tmp_str, "Width=%0.1fmm / Height=%0.1fmm", size_x, size_y);
		gtk_label_set_text(GTK_LABEL(SizeInfoLabel), tmp_str);
		glEndList();
	}

	// save output
	if (save_gcode == 1) {
		if (strcmp(PARAMETER[P_MFILE].vstr, "-") == 0) {
			fd_out = stdout;
		} else {
			fd_out = fopen(PARAMETER[P_MFILE].vstr, "w");
		}
		if (fd_out == NULL) {
			fprintf(stderr, "Can not open file: %s\n", PARAMETER[P_MFILE].vstr);
			exit(0);
		}
		fprintf(fd_out, "%s", gcode_buffer);
		fclose(fd_out);
		if (PARAMETER[P_POST_CMD].vstr[0] != 0) {
			char cmd_str[2048];
			sprintf(cmd_str, PARAMETER[P_POST_CMD].vstr, PARAMETER[P_MFILE].vstr);
			system(cmd_str);
		}
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving g-code...done"), "saving g-code...done");
		save_gcode = 0;
	}


	if (batchmode == 1) {
		onExit();
		exit(0);
	} else {
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearDepth(1.0);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_NORMALIZE);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glPushMatrix();
		glScalef(PARAMETER[P_V_ZOOM].vfloat, PARAMETER[P_V_ZOOM].vfloat, PARAMETER[P_V_ZOOM].vfloat);
		glScalef(scale, scale, scale);
		glTranslatef(PARAMETER[P_V_TRANSX].vint, PARAMETER[P_V_TRANSY].vint, 0.0);
		glRotatef(PARAMETER[P_V_ROTZ].vfloat, 0.0, 0.0, 1.0);
		glRotatef(PARAMETER[P_V_ROTY].vfloat, 0.0, 1.0, 0.0);
		glRotatef(PARAMETER[P_V_ROTX].vfloat, 1.0, 0.0, 0.0);
		glTranslatef(-size_x / 2.0, 0.0, 0.0);
		if (PARAMETER[P_M_ROTARYMODE].vint == 0) {
			glTranslatef(0.0, -size_y / 2.0, 0.0);
		}
		glCallList(1);
		glPopMatrix();
	}
	return;
}


void MaterialLoadList (void) {
/*
    Stahl: 40 – 120 m/min
    Aluminium: 100 – 500 m/min
    Kupfer, Messing und Bronze: 100 – 200 m/min
    Kunststoffe: 50 – 150 m/min
    Holz: Bis zu 3000 m/min
*/

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "Aluminium(Langsp.)", -1);
	material_vc[0] = 200;
	material_fz[0][0] = 0.04;
	material_fz[0][1] = 0.05;
	material_fz[0][2] = 0.10;
	material_texture[0] = "textures/metal.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "Aluminium(Kurzsp.)", -1);
	material_vc[1] = 150;
	material_fz[1][0] = 0.04;
	material_fz[1][1] = 0.05;
	material_fz[1][2] = 0.10;
	material_texture[1] = "textures/metal.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "NE-Metalle", -1);
	material_vc[2] = 150;
	material_fz[2][0] = 0.04;
	material_fz[2][1] = 0.05;
	material_fz[2][2] = 0.10;
	material_texture[2] = "textures/metal.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "VA-Stahl", -1);
	material_vc[3] = 100;
	material_fz[3][0] = 0.05;
	material_fz[3][1] = 0.06;
	material_fz[3][2] = 0.07;
	material_texture[3] = "textures/metal.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "Thermoplaste", -1);
	material_vc[4] = 100;
	material_fz[4][0] = 0.0;
	material_fz[4][1] = 0.0;
	material_fz[4][2] = 0.0;
	material_texture[4] = "textures/plast.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "Duroplaste", -1);
	material_vc[5] = 125;
	material_fz[5][0] = 0.04;
	material_fz[5][1] = 0.08;
	material_fz[5][2] = 0.10;
	material_texture[5] = "textures/plast.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "GFK", -1);
	material_vc[6] = 125;
	material_fz[6][0] = 0.04;
	material_fz[6][1] = 0.08;
	material_fz[6][2] = 0.10;
	material_texture[6] = "textures/gfk.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "CFK", -1);
	material_vc[7] = 125;
	material_fz[7][0] = 0.04;
	material_fz[7][1] = 0.08;
	material_fz[7][2] = 0.10;
	material_texture[7] = "textures/cfk.bmp";

	gtk_list_store_insert_with_values(ListStore[P_MAT_SELECT], NULL, -1, 0, NULL, 1, "Holz", -1);
	material_vc[8] = 2000;
	material_fz[8][0] = 0.04;
	material_fz[8][1] = 0.08;
	material_fz[8][2] = 0.10;
	material_texture[8] = "textures/wood.bmp";



	material_max = 9;
}

void ToolLoadTable (void) {
	/* import Tool-Diameter from Tooltable */
	int n = 0;
	char tmp_str[1024];
	tools_max = 0;
	if (PARAMETER[P_TOOL_TABLE].vstr[0] != 0) {
		FILE *tt_fp;
		char *line = NULL;
		size_t len = 0;
		ssize_t read;
		tt_fp = fopen(PARAMETER[P_TOOL_TABLE].vstr, "r");
		if (tt_fp == NULL) {
			fprintf(stderr, "Can not open Tooltable-File: %s\n", PARAMETER[P_TOOL_TABLE].vstr);
			exit(EXIT_FAILURE);
		}
		tooltbl_diameters[0] = 1;
		n = 0;
		gtk_list_store_clear(ListStore[P_TOOL_SELECT]);
		sprintf(tmp_str, "FREE");
		gtk_list_store_insert_with_values(ListStore[P_TOOL_SELECT], NULL, -1, 0, NULL, 1, tmp_str, -1);
		n++;
		while ((read = getline(&line, &len, tt_fp)) != -1) {
			if (strncmp(line, "T", 1) == 0) {
				char line2[2048];
				trimline(line2, 1024, line);
				int tooln = atoi(line2 + 1);
				double toold = atof(strstr(line2, " D") + 2);
				if (tooln > 0 && tooln < 100 && toold > 0.001) {
					tooltbl_diameters[tooln] = toold;
					tool_descr[tooln][0] = 0;
					if (strstr(line2, ";") > 0) {
						strcpy(tool_descr[tooln], strstr(line2, ";") + 1);
					}
					sprintf(tmp_str, "#%i D%0.2fmm (%s)", tooln, tooltbl_diameters[tooln], tool_descr[tooln]);
					gtk_list_store_insert_with_values(ListStore[P_TOOL_SELECT], NULL, -1, 0, NULL, 1, tmp_str, -1);
					n++;
					tools_max++;
				}
			}
		}
		fclose(tt_fp);
	}
}

void LayerLoadList (void) {
}

void DrawCheckSize (void) {
	int num2 = 0;
	/* check size */
	min_x = 99999.0;
	min_y = 99999.0;
	max_x = 0.0;
	max_y = 0.0;
	for (num2 = 0; num2 < line_last; num2++) {
		if (myLINES[num2].used == 1) {
			if (max_x < myLINES[num2].x1) {
				max_x = myLINES[num2].x1;
			}
			if (max_x < myLINES[num2].x2) {
				max_x = myLINES[num2].x2;
			}
			if (max_y < myLINES[num2].y1) {
				max_y = myLINES[num2].y1;
			}
			if (max_y < myLINES[num2].y2) {
				max_y = myLINES[num2].y2;
			}
			if (myLINES[num2].type == TYPE_CIRCLE && max_x < myLINES[num2].cx + myLINES[num2].opt) {
				max_y = myLINES[num2].cx + myLINES[num2].opt;
			}
			if (myLINES[num2].type == TYPE_CIRCLE && max_y < myLINES[num2].cy + myLINES[num2].opt) {
				max_y = myLINES[num2].cy + myLINES[num2].opt;
			}
			if (min_x > myLINES[num2].x1) {
				min_x = myLINES[num2].x1;
			}
			if (min_x > myLINES[num2].x2) {
				min_x = myLINES[num2].x2;
			}
			if (min_y > myLINES[num2].y1) {
				min_y = myLINES[num2].y1;
			}
			if (min_y > myLINES[num2].y2) {
				min_y = myLINES[num2].y2;
			}
			if (myLINES[num2].type == TYPE_CIRCLE && min_x > myLINES[num2].cx - myLINES[num2].opt) {
				min_x = myLINES[num2].cx - myLINES[num2].opt;
			}
			if (myLINES[num2].type == TYPE_CIRCLE && min_y > myLINES[num2].cy - myLINES[num2].opt) {
				min_y = myLINES[num2].cy - myLINES[num2].opt;
			}
		}
	}
}

void DrawSetZero (void) {
	int num = 0;
	/* set bottom-left to 0,0 */
	for (num = 0; num < line_last; num++) {
		if (myLINES[num].used == 1 || myLINES[num].istab == 1) {
			myLINES[num].x1 -= min_x;
			myLINES[num].y1 -= min_y;
			myLINES[num].x2 -= min_x;
			myLINES[num].y2 -= min_y;
			myLINES[num].cx -= min_x;
			myLINES[num].cy -= min_y;
		}
	}
}

void ArgsRead (int argc, char **argv) {
	int num = 0;
	if (argc < 2) {
//		SetupShowHelp();
//		exit(1);
	}
	mill_layer[0] = 0;
	PARAMETER[P_V_DXF].vstr[0] = 0;
	strcpy(PARAMETER[P_MFILE].vstr, "-");
	for (num = 1; num < argc; num++) {
		if (SetupArgCheck(argv[num], argv[num + 1]) == 1) {
			num++;
		} else if (strcmp(argv[num], "-h") == 0) {
			SetupShowHelp();
			exit(0);
		} else if (num != argc - 1) {
			fprintf(stderr, "### unknown argument: %s ###\n", argv[num]);
			SetupShowHelp();
			exit(1);
		} else {
			strcpy(PARAMETER[P_V_DXF].vstr, argv[argc - 1]);
		}
	}
}

void view_init_gl(void) {
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(glCanvas);
	GdkGLContext *glcontext = gtk_widget_get_gl_context(glCanvas);
	if (gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective((M_PI / 4) / M_PI * 180, (float)width / (float)height, 0.1, 1000.0);
		gluLookAt(0, 0, 6,  0, 0, 0,  0, 1, 0);
		glMatrixMode(GL_MODELVIEW);
		glEnable(GL_NORMALIZE);
		gdk_gl_drawable_gl_end(gldrawable);
	}
}

void view_draw (void) {
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(glCanvas);
	GdkGLContext *glcontext = gtk_widget_get_gl_context(glCanvas);
	if (gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		ParameterUpdate();
		mainloop();
		if (gdk_gl_drawable_is_double_buffered (gldrawable)) {
			gdk_gl_drawable_swap_buffers(gldrawable);
		} else {
			glFlush();
		}
		gdk_gl_drawable_gl_end(gldrawable);
	}
}

void handler_destroy (GtkWidget *widget, gpointer data) {
	gtk_main_quit();
}

void handler_rotate_drawing (GtkWidget *widget, gpointer data) {
	int num;
	for (num = 0; num < line_last; num++) {
		if (myLINES[num].used == 1) {
			double tmp = myLINES[num].x1;
			myLINES[num].x1 = myLINES[num].y1;
			myLINES[num].y1 = size_x - tmp;
			tmp = myLINES[num].x2;
			myLINES[num].x2 = myLINES[num].y2;
			myLINES[num].y2 = size_x - tmp;
			tmp = myLINES[num].cx;
			myLINES[num].cx = myLINES[num].cy;
			myLINES[num].cy = size_x - tmp;
		}
	}
	init_objects();
	DrawCheckSize();
	DrawSetZero();

}

void handler_load_dxf (GtkWidget *widget, gpointer data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Load Drawing",
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	GtkFileFilter *ffilter;
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, "DXF-Drawings");
	gtk_file_filter_add_pattern(ffilter, "*.dxf");
	gtk_file_filter_add_pattern(ffilter, "*.DXF");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
/*
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, "DWG-Drawings");
	gtk_file_filter_add_pattern(ffilter, "*.dwg");
	gtk_file_filter_add_pattern(ffilter, "*.DXF");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
	
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, "SVG-Drawings");
	gtk_file_filter_add_pattern(ffilter, "*.scg");
	gtk_file_filter_add_pattern(ffilter, "*.SVG");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
*/
	if (PARAMETER[P_TOOL_TABLE].vstr[0] == 0) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), "./");
	} else {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), PARAMETER[P_V_DXF].vstr);
	}
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		strcpy(PARAMETER[P_V_DXF].vstr, filename);
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "reading dxf..."), "reading dxf...");
		loading = 1;
		dxf_read(PARAMETER[P_V_DXF].vstr);
		init_objects();
		DrawCheckSize();
		DrawSetZero();
		loading = 0;
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "reading dxf...done"), "reading dxf...done");
		g_free(filename);
	}
	gtk_widget_destroy (dialog);
}

void handler_load_preset (GtkWidget *widget, gpointer data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Load preset",
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	GtkFileFilter *ffilter;
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, "Preset");
	gtk_file_filter_add_pattern(ffilter, "*.preset");
	gtk_file_filter_add_pattern(ffilter, "*.PRESET");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "./");
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "reading preset..."), "reading preset...");
		SetupLoadPreset(filename);
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "reading preset...done"), "reading preset...done");
		g_free(filename);
	}
	gtk_widget_destroy (dialog);
}

void handler_save_preset (GtkWidget *widget, gpointer data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Save preset",
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	GtkFileFilter *ffilter;
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, "Preset");
	gtk_file_filter_add_pattern(ffilter, "*.preset");
	gtk_file_filter_add_pattern(ffilter, "*.PRESET");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "./");
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving Preset..."), "saving Preset...");
		SetupSavePreset(filename);
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving Preset...done"), "saving Preset...done");
		g_free(filename);
	}
	gtk_widget_destroy (dialog);
}

char *suffix_remove (char *mystr) {
	char *retstr;
	char *lastdot;
	if (mystr == NULL) {
		return NULL;
	}
	if ((retstr = malloc (strlen (mystr) + 1)) == NULL) {
        	return NULL;
	}
	strcpy(retstr, mystr);
	lastdot = strrchr(retstr, '.');
	if (lastdot != NULL) {
		*lastdot = '\0';
	}
	return retstr;
}

void handler_save_lua (GtkWidget *widget, gpointer data) {
	gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving lua..."), "saving lua...");
	postcam_save_source(postcam_plugins[PARAMETER[P_H_POST].vint]);
	postcam_plugin = -1;
	gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving lua...done"), "saving lua...done");
}

void handler_save_gcode (GtkWidget *widget, gpointer data) {
	char ext_str[1024];
	GtkWidget *dialog;
	sprintf(ext_str, "Save Output (.%s)", output_extension);
	dialog = gtk_file_chooser_dialog_new (ext_str,
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	GtkFileFilter *ffilter;
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, output_extension);
	sprintf(ext_str, "*.%s", output_extension);
	gtk_file_filter_add_pattern(ffilter, ext_str);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
	if (PARAMETER[P_MFILE].vstr[0] == 0) {
		char dir[2048];
		strcpy(dir, PARAMETER[P_V_DXF].vstr);
		dirname(dir);
		char file[2048];
		strcpy(file, basename(PARAMETER[P_V_DXF].vstr));
		char *file_nosuffix = suffix_remove(file);
		file_nosuffix = realloc(file_nosuffix, strlen(file_nosuffix) + 5);
		strcat(file_nosuffix, ".");
		strcat(file_nosuffix, output_extension);
		if (strstr(PARAMETER[P_V_DXF].vstr, "/") > 0) {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);
		} else {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), "./");
		}

		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), file_nosuffix);
		free(file_nosuffix);
	} else {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER (dialog), PARAMETER[P_MFILE].vstr);
	}
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		strcpy(PARAMETER[P_MFILE].vstr, filename);
		g_free(filename);
		save_gcode = 1;
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving g-code..."), "saving g-code...");
	}
	gtk_widget_destroy (dialog);
}

void handler_load_tooltable (GtkWidget *widget, gpointer data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Load Tooltable",
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	GtkFileFilter *ffilter;
	ffilter = gtk_file_filter_new();
	gtk_file_filter_set_name(ffilter, "Tooltable");
	gtk_file_filter_add_pattern(ffilter, "*.tbl");
	gtk_file_filter_add_pattern(ffilter, "*.TBL");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);

	if (PARAMETER[P_TOOL_TABLE].vstr[0] == 0) {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "/tmp");
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "tool.tbl");
	} else {
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), PARAMETER[P_TOOL_TABLE].vstr);
	}
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		strcpy(PARAMETER[P_TOOL_TABLE].vstr, filename);
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "loading tooltable..."), "loading tooltable...");
		ToolLoadTable();
		gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "loading tooltable...done"), "loading tooltable...done");
		g_free(filename);
	}
	gtk_widget_destroy (dialog);
}

void handler_save_setup (GtkWidget *widget, gpointer data) {
	gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving setup..."), "saving setup...");
	SetupSave();
	gtk_statusbar_push(GTK_STATUSBAR(StatusBar), gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "saving setup...done"), "saving setup...done");
}

void handler_about (GtkWidget *widget, gpointer data) {
	GtkWidget *dialog = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_title(GTK_WINDOW(dialog), "About");
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_QUIT, 1);
	GtkWidget *label = gtk_label_new("CAMmill 2D\nCopyright by Oliver Dippel <oliver@multixmedia.org>\nMac-Port by McUles <mcules@fpv-club.de>");
	gtk_widget_modify_font(label, pango_font_description_from_string("Tahoma 18"));
	GtkWidget *image = gtk_image_new_from_file("icons/logo.png");
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_pack_start(GTK_BOX(box), image, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);
}

void handler_draw (GtkWidget *w, GdkEventExpose* e, void *v) {
}

void handler_scrollwheel (GtkWidget *w, GdkEventButton* e, void *v) {
	if (e->state == 0) {
		PARAMETER[P_V_ZOOM].vfloat += 0.05;
	} else if (e->state == 1) {
		PARAMETER[P_V_ZOOM].vfloat -= 0.05;
	}
}

void handler_button_press (GtkWidget *w, GdkEventButton* e, void *v) {
//	printf("button_press x=%g y=%g b=%d state=%d\n", e->x, e->y, e->button, e->state);
	int mouseX = e->x;
	int mouseY = e->y;
	int state = e->state;
	int button = e->button;;

	if (button == 4 && state == 0) {
		PARAMETER[P_V_ZOOM].vfloat += 0.05;
	} else if (button == 5 && state == 0) {
		PARAMETER[P_V_ZOOM].vfloat -= 0.05;
	} else if (button == 1) {
		if (state == 0) {
			last_mouse_x = mouseX - PARAMETER[P_V_TRANSX].vint * 2;
			last_mouse_y = mouseY - PARAMETER[P_V_TRANSY].vint * -2;
			last_mouse_button = button;
			last_mouse_state = state;
		} else {
			last_mouse_button = button;
			last_mouse_state = state;
		}
	} else if (button == 2) {
		if (state == 0) {
			last_mouse_x = mouseX - (int)(PARAMETER[P_V_ROTY].vfloat * 5.0);
			last_mouse_y = mouseY - (int)(PARAMETER[P_V_ROTX].vfloat * 5.0);
			last_mouse_button = button;
			last_mouse_state = state;
		} else {
			last_mouse_button = button;
			last_mouse_state = state;
		}
	} else if (button == 3) {
		if (state == 0) {
			last_mouse_x = mouseX - (int)(PARAMETER[P_V_ROTZ].vfloat * 5.0);;
			last_mouse_y = mouseY - (int)(PARAMETER[P_V_ZOOM].vfloat * 100.0);
			last_mouse_button = button;
			last_mouse_state = state;
		} else {
			last_mouse_button = button;
			last_mouse_state = state;
		}
	}
}

void handler_button_release (GtkWidget *w, GdkEventButton* e, void *v) {
//	printf("button_release x=%g y=%g b=%d state=%d\n", e->x, e->y, e->button, e->state);
	last_mouse_button = -1;
}

void handler_motion (GtkWidget *w, GdkEventMotion* e, void *v) {
//	printf("button_motion x=%g y=%g state=%d\n", e->x, e->y, e->state);
	int mouseX = e->x;
	int mouseY = e->y;
	if (last_mouse_button == 1) {
		PARAMETER[P_V_TRANSX].vint = (mouseX - last_mouse_x) / 2;
		PARAMETER[P_V_TRANSY].vint = (mouseY - last_mouse_y) / -2;
	} else if (last_mouse_button == 2) {
		PARAMETER[P_V_ROTY].vfloat = (float)(mouseX - last_mouse_x) / 5.0;
		PARAMETER[P_V_ROTX].vfloat = (float)(mouseY - last_mouse_y) / 5.0;
	} else if (last_mouse_button == 3) {
		PARAMETER[P_V_ROTZ].vfloat = (float)(mouseX - last_mouse_x) / 5.0;
		PARAMETER[P_V_ZOOM].vfloat = (float)(mouseY - last_mouse_y) / 100;
	} else {
		last_mouse_x = mouseX;
		last_mouse_y = mouseY;
	}
}

void handler_keypress (GtkWidget *w, GdkEventKey* e, void *v) {
//	printf("key_press state=%d key=%s\n", e->state, e->string);
}

void handler_configure (GtkWidget *w, GdkEventConfigure* e, void *v) {
//	printf("configure width=%d height=%d ratio=%g\n", e->width, e->height, e->width/(float)e->height);
	width = e->width;
	height = e->height;
	need_init = 1;
}

int handler_periodic_action (gpointer d) {
	while ( gtk_events_pending() ) {
		gtk_main_iteration();
	}
	if (need_init == 1) {
		need_init = 0;
		view_init_gl();
	}
	view_draw();
	return(1);
}

GtkWidget *create_gl () {
	static GdkGLConfig *glconfig = NULL;
	static GdkGLContext *glcontext = NULL;
	GtkWidget *area;
	if (glconfig == NULL) {
		glconfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE);
		if (glconfig == NULL) {
			glconfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH);
			if (glconfig == NULL) {
				exit(1);
			}
		}
	}
	area = gtk_drawing_area_new();
	gtk_widget_set_gl_capability(area, glconfig, glcontext, TRUE, GDK_GL_RGBA_TYPE); 
	gtk_widget_set_events(GTK_WIDGET(area)
		,GDK_POINTER_MOTION_MASK 
		|GDK_BUTTON_PRESS_MASK 
		|GDK_BUTTON_RELEASE_MASK
		|GDK_ENTER_NOTIFY_MASK
		|GDK_KEY_PRESS_MASK
		|GDK_KEY_RELEASE_MASK
		|GDK_EXPOSURE_MASK
	);
	gtk_signal_connect(GTK_OBJECT(area), "enter-notify-event", GTK_SIGNAL_FUNC(gtk_widget_grab_focus), NULL);
	return(area);
}

void ParameterChanged (GtkWidget *widget, gpointer data) {
	if (loading == 1) {
		return;
	}
	int n = (int)data;
//	printf("UPDATED(%i): %s\n", n, PARAMETER[n].name);
	if (PARAMETER[n].type == T_FLOAT) {
		PARAMETER[n].vfloat = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
	} else if (PARAMETER[n].type == T_DOUBLE) {
		PARAMETER[n].vdouble = (double)gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
	} else if (PARAMETER[n].type == T_INT) {
		PARAMETER[n].vint = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
	} else if (PARAMETER[n].type == T_SELECT) {
		PARAMETER[n].vint = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	} else if (PARAMETER[n].type == T_BOOL) {
		PARAMETER[n].vint = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	} else if (PARAMETER[n].type == T_STRING) {
		strcpy(PARAMETER[n].vstr, (char *)gtk_entry_get_text(GTK_ENTRY(widget)));
	} else if (PARAMETER[n].type == T_FILE) {
		strcpy(PARAMETER[n].vstr, (char *)gtk_entry_get_text(GTK_ENTRY(widget)));
	}

	if (n == P_O_SELECT) {
		int object_num = PARAMETER[P_O_SELECT].vint;
		PARAMETER[P_O_USE].vint = myOBJECTS[object_num].use;
		PARAMETER[P_O_FORCE].vint = myOBJECTS[object_num].force;
		PARAMETER[P_O_CLIMB].vint = myOBJECTS[object_num].climb;
		PARAMETER[P_O_OFFSET].vint = myOBJECTS[object_num].offset;
		PARAMETER[P_O_OVERCUT].vint = myOBJECTS[object_num].overcut;
		PARAMETER[P_O_POCKET].vint = myOBJECTS[object_num].pocket;
		PARAMETER[P_O_LASER].vint = myOBJECTS[object_num].laser;
		PARAMETER[P_O_DEPTH].vdouble = myOBJECTS[object_num].depth;
	} else if (n > P_O_SELECT) {
		int object_num = PARAMETER[P_O_SELECT].vint;
		myOBJECTS[object_num].use = PARAMETER[P_O_USE].vint;
		myOBJECTS[object_num].force = PARAMETER[P_O_FORCE].vint;
		myOBJECTS[object_num].climb = PARAMETER[P_O_CLIMB].vint;
		myOBJECTS[object_num].offset = PARAMETER[P_O_OFFSET].vint;
		myOBJECTS[object_num].overcut = PARAMETER[P_O_OVERCUT].vint;
		myOBJECTS[object_num].pocket = PARAMETER[P_O_POCKET].vint;
		myOBJECTS[object_num].laser = PARAMETER[P_O_LASER].vint;
		myOBJECTS[object_num].depth = PARAMETER[P_O_DEPTH].vdouble;
	}

	if (strncmp(PARAMETER[n].name, "Translate", 9) != 0 && strncmp(PARAMETER[n].name, "Rotate", 6) != 0 && strncmp(PARAMETER[n].name, "Zoom", 4) != 0) {
		update_post = 1;
	}
}

void ParameterUpdate (void) {
	char path[1024];
	char value2[1024];
	int n = 0;
	for (n = 0; n < P_LAST; n++) {
		if (PARAMETER[n].type == T_FLOAT) {
			if (PARAMETER[n].vfloat != gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(ParamValue[n]))) {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(ParamValue[n]), PARAMETER[n].vfloat);
			}
		} else if (PARAMETER[n].type == T_DOUBLE) {
			if (PARAMETER[n].vdouble != gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(ParamValue[n]))) {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(ParamValue[n]), PARAMETER[n].vdouble);
			}
		} else if (PARAMETER[n].type == T_INT) {
			if (PARAMETER[n].vint != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ParamValue[n]))) {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(ParamValue[n]), PARAMETER[n].vint);
			}
		} else if (PARAMETER[n].type == T_SELECT) {
			if (PARAMETER[n].vint != gtk_combo_box_get_active(GTK_COMBO_BOX(ParamValue[n]))) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(ParamValue[n]), PARAMETER[n].vint);
			}
		} else if (PARAMETER[n].type == T_BOOL) {
			if (PARAMETER[n].vint != gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ParamValue[n]))) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ParamValue[n]), PARAMETER[n].vint);
			}
		} else if (PARAMETER[n].type == T_STRING) {
			gtk_entry_set_text(GTK_ENTRY(ParamValue[n]), PARAMETER[n].vstr);
		} else if (PARAMETER[n].type == T_FILE) {
			gtk_entry_set_text(GTK_ENTRY(ParamValue[n]), PARAMETER[n].vstr);
		} else {
			continue;
		}
	}
	for (n = 0; n < P_LAST; n++) {
		sprintf(path, "0:%i:%i", PARAMETER[n].l1, PARAMETER[n].l2);
		if (PARAMETER[n].type == T_FLOAT) {
			sprintf(value2, "%f", PARAMETER[n].vfloat);
		} else if (PARAMETER[n].type == T_DOUBLE) {
			sprintf(value2, "%f", PARAMETER[n].vdouble);
		} else if (PARAMETER[n].type == T_INT) {
			sprintf(value2, "%i", PARAMETER[n].vint);
		} else if (PARAMETER[n].type == T_SELECT) {
			sprintf(value2, "%i", PARAMETER[n].vint);
		} else if (PARAMETER[n].type == T_BOOL) {
			sprintf(value2, "%i", PARAMETER[n].vint);
		} else if (PARAMETER[n].type == T_STRING) {
			sprintf(value2, "%s", PARAMETER[n].vstr);
		} else if (PARAMETER[n].type == T_FILE) {
			sprintf(value2, "%s", PARAMETER[n].vstr);
		} else {
			continue;
		}
	}
}



GdkPixbuf *create_pixbuf(const gchar * filename) {
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if(!pixbuf) {
		fprintf(stderr, "%s\n", error->message);
		g_error_free(error);
	}
	return pixbuf;
}


#include <locale.h>

int main (int argc, char *argv[]) {

	// force dots in printf
	setlocale(LC_NUMERIC, "C");

	SetupLoad();
	ArgsRead(argc, argv);
	SetupShow();

	GtkWidget *hbox;
	GtkWidget *vbox;
	gtk_init(&argc, &argv);

	gtk_gl_init(&argc, &argv);


	glCanvas = create_gl();
	gtk_widget_set_usize(GTK_WIDGET(glCanvas), 800, 600);
	gtk_signal_connect(GTK_OBJECT(glCanvas), "expose_event", GTK_SIGNAL_FUNC(handler_draw), NULL);  
	gtk_signal_connect(GTK_OBJECT(glCanvas), "button_press_event", GTK_SIGNAL_FUNC(handler_button_press), NULL);  
	gtk_signal_connect(GTK_OBJECT(glCanvas), "button_release_event", GTK_SIGNAL_FUNC(handler_button_release), NULL);  
	gtk_signal_connect(GTK_OBJECT(glCanvas), "configure_event", GTK_SIGNAL_FUNC(handler_configure), NULL);  
	gtk_signal_connect(GTK_OBJECT(glCanvas), "motion_notify_event", GTK_SIGNAL_FUNC(handler_motion), NULL);  
	gtk_signal_connect(GTK_OBJECT(glCanvas), "key_press_event", GTK_SIGNAL_FUNC(handler_keypress), NULL);  
	gtk_signal_connect(GTK_OBJECT(glCanvas), "scroll-event", GTK_SIGNAL_FUNC(handler_scrollwheel), NULL);  

	// top-menu
	GtkWidget *MenuBar = gtk_menu_bar_new();
	GtkWidget *MenuItem;
	GtkWidget *FileMenu = gtk_menu_item_new_with_label("File");
	GtkWidget *FileMenuList = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(FileMenu), FileMenuList);
	gtk_menu_bar_append(GTK_MENU_BAR(MenuBar), FileMenu);

		MenuItem = gtk_menu_item_new_with_label("Load DXF");
		gtk_menu_append(GTK_MENU(FileMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_load_dxf), NULL);

		MenuItem = gtk_menu_item_new_with_label("Load Preset");
		gtk_menu_append(GTK_MENU(FileMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_load_preset), NULL);

		MenuItem = gtk_menu_item_new_with_label("Save Output");
		gtk_menu_append(GTK_MENU(FileMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_save_gcode), NULL);

		MenuItem = gtk_menu_item_new_with_label("Load Tooltable");
		gtk_menu_append(GTK_MENU(FileMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_load_tooltable), NULL);

		MenuItem = gtk_menu_item_new_with_label("Save Setup");
		gtk_menu_append(GTK_MENU(FileMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_save_setup), NULL);

		MenuItem = gtk_menu_item_new_with_label("Quit");
		gtk_menu_append(GTK_MENU(FileMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_destroy), NULL);


	GtkWidget *HelpMenu = gtk_menu_item_new_with_label("Help");
	GtkWidget *HelpMenuList = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(HelpMenu), HelpMenuList);
	gtk_menu_bar_append(GTK_MENU_BAR(MenuBar), HelpMenu);

		MenuItem = gtk_menu_item_new_with_label("About");
		gtk_menu_append(GTK_MENU(HelpMenuList), MenuItem);
		gtk_signal_connect(GTK_OBJECT(MenuItem), "activate", GTK_SIGNAL_FUNC(handler_about), NULL);


	GtkWidget *ToolBar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(ToolBar), GTK_TOOLBAR_ICONS);

	GtkToolItem *ToolItemLoadDXF = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_tool_item_set_tooltip_text(ToolItemLoadDXF, "Load DXF");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemLoadDXF, -1);
	g_signal_connect(G_OBJECT(ToolItemLoadDXF), "clicked", GTK_SIGNAL_FUNC(handler_load_dxf), NULL);

	GtkToolItem *ToolItemSaveGcode = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_tool_item_set_tooltip_text(ToolItemSaveGcode, "Save Output");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemSaveGcode, -1);
	g_signal_connect(G_OBJECT(ToolItemSaveGcode), "clicked", GTK_SIGNAL_FUNC(handler_save_gcode), NULL);

	GtkToolItem *ToolItemSaveSetup = gtk_tool_button_new_from_stock(GTK_STOCK_PROPERTIES);
	gtk_tool_item_set_tooltip_text(ToolItemSaveSetup, "Save Setup");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemSaveSetup, -1);
	g_signal_connect(G_OBJECT(ToolItemSaveSetup), "clicked", GTK_SIGNAL_FUNC(handler_save_setup), NULL);

	GtkToolItem *ToolItemSep = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemSep, -1); 

	GtkToolItem *ToolItemLoadPreset = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_tool_item_set_tooltip_text(ToolItemLoadPreset, "Load Preset");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemLoadPreset, -1);
	g_signal_connect(G_OBJECT(ToolItemLoadPreset), "clicked", GTK_SIGNAL_FUNC(handler_load_preset), NULL);

	GtkToolItem *ToolItemSavePreset = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_tool_item_set_tooltip_text(ToolItemSavePreset, "Save Preset");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemSavePreset, -1);
	g_signal_connect(G_OBJECT(ToolItemSavePreset), "clicked", GTK_SIGNAL_FUNC(handler_save_preset), NULL);


	GtkToolItem *ToolItemSep1 = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemSep1, -1); 


	GtkToolItem *TB;
	TB = gtk_tool_button_new_from_stock(GTK_STOCK_CONVERT);
	gtk_tool_item_set_tooltip_text(TB, "Rotate 90°");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), TB, -1);
	g_signal_connect(G_OBJECT(TB), "clicked", GTK_SIGNAL_FUNC(handler_rotate_drawing), NULL);

	GtkToolItem *ToolItemSep2 = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemSep2, -1); 
/*
	GtkToolItem *ToolItemExit = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);
	gtk_tool_item_set_tooltip_text(ToolItemExit, "Quit");
	gtk_toolbar_insert(GTK_TOOLBAR(ToolBar), ToolItemExit, -1);
	g_signal_connect(G_OBJECT(ToolItemExit), "clicked", GTK_SIGNAL_FUNC(handler_destroy), NULL);
*/
	GtkWidget *NbBox = gtk_table_new(2, 2, FALSE);
	GtkWidget *notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_table_attach_defaults(GTK_TABLE(NbBox), notebook, 0, 1, 0, 1);

	int ViewNum = 0;
	GtkWidget *ViewLabel = gtk_label_new("View");
	GtkWidget *ViewBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ViewBox, ViewLabel);

	int ToolNum = 0;
	GtkWidget *ToolLabel = gtk_label_new("Tool");
	GtkWidget *ToolBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ToolBox, ToolLabel);

	int MillingNum = 0;
	GtkWidget *MillingLabel = gtk_label_new("Milling");
	GtkWidget *MillingBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), MillingBox, MillingLabel);

	int HoldingNum = 0;
	GtkWidget *HoldingLabel = gtk_label_new("Tabs");
	GtkWidget *HoldingBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), HoldingBox, HoldingLabel);

	int RotaryNum = 0;
	GtkWidget *RotaryLabel = gtk_label_new("Rotary");
	GtkWidget *RotaryBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), RotaryBox, RotaryLabel);

	int TangencialNum = 0;
	GtkWidget *TangencialLabel = gtk_label_new("Tangencial");
	GtkWidget *TangencialBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), TangencialBox, TangencialLabel);

	int MachineNum = 0;
	GtkWidget *MachineLabel = gtk_label_new("Machine");
	GtkWidget *MachineBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), MachineBox, MachineLabel);

	int MaterialNum = 0;
	GtkWidget *MaterialLabel = gtk_label_new("Material");
	GtkWidget *MaterialBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), MaterialBox, MaterialLabel);

	int ObjectsNum = 0;
	GtkWidget *ObjectsLabel = gtk_label_new("Objects");
	GtkWidget *ObjectsBox = gtk_vbox_new(0, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ObjectsBox, ObjectsLabel);


	int n = 0;
	for (n = 0; n < P_LAST; n++) {
		GtkWidget *Label;
		GtkTooltips *tooltips = gtk_tooltips_new ();
		GtkWidget *Box = gtk_hbox_new(0, 0);
		if (PARAMETER[n].type == T_FLOAT) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			GtkAdjustment *Adj = (GtkAdjustment *)gtk_adjustment_new(PARAMETER[n].vdouble, PARAMETER[n].min, PARAMETER[n].max, PARAMETER[n].step, PARAMETER[n].step * 10.0, 0.0);
			ParamValue[n] = gtk_spin_button_new(Adj, PARAMETER[n].step, 3);
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
		} else if (PARAMETER[n].type == T_DOUBLE) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			GtkAdjustment *Adj = (GtkAdjustment *)gtk_adjustment_new(PARAMETER[n].vdouble, PARAMETER[n].min, PARAMETER[n].max, PARAMETER[n].step, PARAMETER[n].step * 10.0, 0.0);
			ParamValue[n] = gtk_spin_button_new(Adj, PARAMETER[n].step, 3);
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
		} else if (PARAMETER[n].type == T_INT) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			GtkAdjustment *Adj = (GtkAdjustment *)gtk_adjustment_new(PARAMETER[n].vdouble, PARAMETER[n].min, PARAMETER[n].max, PARAMETER[n].step, PARAMETER[n].step * 10.0, 0.0);
			ParamValue[n] = gtk_spin_button_new(Adj, PARAMETER[n].step, 3);
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
		} else if (PARAMETER[n].type == T_SELECT) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			ListStore[n] = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
			ParamValue[n] = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ListStore[n]));
			g_object_unref(ListStore[n]);
			GtkCellRenderer *column;
			column = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(ParamValue[n]), column, TRUE);
			gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(ParamValue[n]), column, "cell-background", 0, "text", 1, NULL);
			gtk_combo_box_set_active(GTK_COMBO_BOX(ParamValue[n]), 1);
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
		} else if (PARAMETER[n].type == T_BOOL) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			ParamValue[n] = gtk_check_button_new_with_label("On/Off");
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
		} else if (PARAMETER[n].type == T_STRING) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			ParamValue[n] = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(ParamValue[n]), PARAMETER[n].vstr);
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
		} else if (PARAMETER[n].type == T_FILE) {
			Label = gtk_label_new(PARAMETER[n].name);
			GtkWidget *Align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(Align), Label);
			ParamValue[n] = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(ParamValue[n]), PARAMETER[n].vstr);
			ParamButton[n] = gtk_button_new();
			GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
			gtk_button_set_image(GTK_BUTTON(ParamButton[n]), image);
			gtk_box_pack_start(GTK_BOX(Box), Align, 1, 1, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamValue[n], 0, 0, 0);
			gtk_box_pack_start(GTK_BOX(Box), ParamButton[n], 0, 0, 0);
		} else {
			continue;
		}
		gtk_tooltips_set_tip(tooltips, Box, PARAMETER[n].help, NULL);
		if (strcmp(PARAMETER[n].group, "View") == 0) {
			gtk_box_pack_start(GTK_BOX(ViewBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 0;
			PARAMETER[n].l2 = ViewNum;
			ViewNum++;
		} else if (strcmp(PARAMETER[n].group, "Tool") == 0) {
			gtk_box_pack_start(GTK_BOX(ToolBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 1;
			PARAMETER[n].l2 = ToolNum;
			ToolNum++;
		} else if (strcmp(PARAMETER[n].group, "Milling") == 0) {
			gtk_box_pack_start(GTK_BOX(MillingBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 2;
			PARAMETER[n].l2 = MillingNum;
			MillingNum++;
		} else if (strcmp(PARAMETER[n].group, "Holding-Tabs") == 0) {
			gtk_box_pack_start(GTK_BOX(HoldingBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 3;
			PARAMETER[n].l2 = HoldingNum;
			HoldingNum++;
		} else if (strcmp(PARAMETER[n].group, "Rotary") == 0) {
			gtk_box_pack_start(GTK_BOX(RotaryBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 3;
			PARAMETER[n].l2 = RotaryNum;
			RotaryNum++;
		} else if (strcmp(PARAMETER[n].group, "Tangencial") == 0) {
			gtk_box_pack_start(GTK_BOX(TangencialBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 3;
			PARAMETER[n].l2 = TangencialNum;
			TangencialNum++;
		} else if (strcmp(PARAMETER[n].group, "Machine") == 0) {
			gtk_box_pack_start(GTK_BOX(MachineBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 4;
			PARAMETER[n].l2 = MachineNum;
			MachineNum++;
		} else if (strcmp(PARAMETER[n].group, "Material") == 0) {
			gtk_box_pack_start(GTK_BOX(MaterialBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 5;
			PARAMETER[n].l2 = MaterialNum;
			MaterialNum++;
		} else if (strcmp(PARAMETER[n].group, "Objects") == 0) {
			gtk_box_pack_start(GTK_BOX(ObjectsBox), Box, 0, 0, 0);
			PARAMETER[n].l1 = 6;
			PARAMETER[n].l2 = ObjectsNum;
			ObjectsNum++;
		}
	}

	OutputInfoLabel = gtk_label_new("-- OutputInfo --");
	gtk_box_pack_start(GTK_BOX(MachineBox), OutputInfoLabel, 0, 0, 0);

	/* import DXF */
	loading = 1;
	if (PARAMETER[P_V_DXF].vstr[0] != 0) {
		dxf_read(PARAMETER[P_V_DXF].vstr);
	}
	init_objects();
	DrawCheckSize();
	DrawSetZero();
//	LayerLoadList();
	loading = 0;


	gtk_list_store_insert_with_values(ListStore[P_O_OFFSET], NULL, -1, 0, NULL, 1, "None", -1);
	gtk_list_store_insert_with_values(ListStore[P_O_OFFSET], NULL, -1, 0, NULL, 1, "Inside", -1);
	gtk_list_store_insert_with_values(ListStore[P_O_OFFSET], NULL, -1, 0, NULL, 1, "Outside", -1);

	gtk_list_store_insert_with_values(ListStore[P_H_ROTARYAXIS], NULL, -1, 0, NULL, 1, "A", -1);
	gtk_list_store_insert_with_values(ListStore[P_H_ROTARYAXIS], NULL, -1, 0, NULL, 1, "B", -1);
	gtk_list_store_insert_with_values(ListStore[P_H_ROTARYAXIS], NULL, -1, 0, NULL, 1, "C", -1);

	gtk_list_store_insert_with_values(ListStore[P_H_KNIFEAXIS], NULL, -1, 0, NULL, 1, "A", -1);
	gtk_list_store_insert_with_values(ListStore[P_H_KNIFEAXIS], NULL, -1, 0, NULL, 1, "B", -1);
	gtk_list_store_insert_with_values(ListStore[P_H_KNIFEAXIS], NULL, -1, 0, NULL, 1, "C", -1);

	DIR *dir;
	n = 0;
	struct dirent *ent;
	if ((dir = opendir("posts/")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] != '.') {
				char *pname = suffix_remove(ent->d_name);
				gtk_list_store_insert_with_values(ListStore[P_H_POST], NULL, -1, 0, NULL, 1, pname, -1);
				strcpy(postcam_plugins[n++], pname);
				postcam_plugins[n][0] = 0;
				free(pname);
			}
		}
		closedir (dir);
	} else {
		fprintf(stderr, "postprocessor directory not found: posts/\n");
	}

/*
	if ((dir = opendir("fonts/")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] != '.') {
				char *pname = suffix_remove(ent->d_name);
				gtk_list_store_insert_with_values(ListStore[P_M_FONT], NULL, -1, 0, NULL, 1, pname, -1);
				free(pname);
			}
		}
		closedir (dir);
	} else {
		fprintf(stderr, "fonts directory not found: fonts/\n");
	}
*/

	g_signal_connect(G_OBJECT(ParamButton[P_MFILE]), "clicked", GTK_SIGNAL_FUNC(handler_save_gcode), NULL);
	g_signal_connect(G_OBJECT(ParamButton[P_TOOL_TABLE]), "clicked", GTK_SIGNAL_FUNC(handler_load_tooltable), NULL);
	g_signal_connect(G_OBJECT(ParamButton[P_V_DXF]), "clicked", GTK_SIGNAL_FUNC(handler_load_dxf), NULL);

	MaterialLoadList();
	ToolLoadTable();

	ParameterUpdate();
	for (n = 0; n < P_LAST; n++) {
		if (PARAMETER[n].type == T_FLOAT) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "changed", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		} else if (PARAMETER[n].type == T_DOUBLE) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "changed", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		} else if (PARAMETER[n].type == T_INT) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "changed", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		} else if (PARAMETER[n].type == T_SELECT) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "changed", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		} else if (PARAMETER[n].type == T_BOOL) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "toggled", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		} else if (PARAMETER[n].type == T_STRING) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "changed", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		} else if (PARAMETER[n].type == T_FILE) {
			gtk_signal_connect(GTK_OBJECT(ParamValue[n]), "changed", GTK_SIGNAL_FUNC(ParameterChanged), (gpointer)n);
		}
	}


	StatusBar = gtk_statusbar_new();

	gCodeViewLua = gtk_source_view_new();
	gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(gCodeViewLua), TRUE);
	gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(gCodeViewLua), TRUE);
	gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(gCodeViewLua), TRUE);
//	gtk_source_view_set_right_margin_position(GTK_SOURCE_VIEW(gCodeViewLua), 80);
//	gtk_source_view_set_show_right_margin(GTK_SOURCE_VIEW(gCodeViewLua), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gCodeViewLua), GTK_WRAP_WORD_CHAR);

	GtkWidget *textWidgetLua = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(textWidgetLua), gCodeViewLua);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(textWidgetLua), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);


	GtkWidget *LuaToolBar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(LuaToolBar), GTK_TOOLBAR_ICONS);

	GtkToolItem *LuaToolItemSaveGcode = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_tool_item_set_tooltip_text(LuaToolItemSaveGcode, "Save Output");
	gtk_toolbar_insert(GTK_TOOLBAR(LuaToolBar), LuaToolItemSaveGcode, -1);
	g_signal_connect(G_OBJECT(LuaToolItemSaveGcode), "clicked", GTK_SIGNAL_FUNC(handler_save_lua), NULL);

	GtkToolItem *LuaToolItemSep = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(LuaToolBar), LuaToolItemSep, -1); 

	OutputErrorLabel = gtk_label_new("-- OutputErrors --");

	GtkWidget *textWidgetLuaBox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(textWidgetLuaBox), LuaToolBar, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(textWidgetLuaBox), textWidgetLua, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(textWidgetLuaBox), OutputErrorLabel, 0, 0, 0);

	GtkTextBuffer *bufferLua;
	const gchar *textLua = "Hallo Lua";
	bufferLua = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCodeViewLua));
	gtk_text_buffer_set_text(bufferLua, textLua, -1);

	gCodeView = gtk_source_view_new();
	gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(gCodeView), TRUE);
	gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(gCodeView), TRUE);
	gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(gCodeView), TRUE);
//	gtk_source_view_set_right_margin_position(GTK_SOURCE_VIEW(gCodeView), 80);
//	gtk_source_view_set_show_right_margin(GTK_SOURCE_VIEW(gCodeView), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gCodeView), GTK_WRAP_WORD_CHAR);

	GtkWidget *textWidget = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(textWidget), gCodeView);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(textWidget), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	GtkTextBuffer *buffer;
	const gchar *text = "Hallo Text";
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCodeView));
	gtk_text_buffer_set_text(buffer, text, -1);

	GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default();
	const gchar * const *current;
	int i;
	current = gtk_source_language_manager_get_search_path(manager);
	for (i = 0; current[i] != NULL; i++) {}
	gchar **lang_dirs;
	lang_dirs = g_new0(gchar *, i + 2);
	for (i = 0; current[i] != NULL; i++) {
		lang_dirs[i] = g_build_filename(current[i], NULL);
	}
	lang_dirs[i] = g_build_filename(".", NULL);
	gtk_source_language_manager_set_search_path(manager, lang_dirs);
	g_strfreev(lang_dirs);
	GtkSourceLanguage *ngclang = gtk_source_language_manager_get_language(manager, ".ngc");
	gtk_source_buffer_set_language(GTK_SOURCE_BUFFER(buffer), ngclang);

	GtkSourceLanguage *langLua = gtk_source_language_manager_get_language(manager, "lua");
	gtk_source_buffer_set_language(GTK_SOURCE_BUFFER(bufferLua), langLua);


	GtkWidget *NbBox2 = gtk_table_new(2, 2, FALSE);
	GtkWidget *notebook2 = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook2), GTK_POS_TOP);
	gtk_table_attach_defaults(GTK_TABLE(NbBox2), notebook2, 0, 1, 0, 1);

	GtkWidget *glCanvasLabel = gtk_label_new("3D-View");
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook2), glCanvas, glCanvasLabel);

	gCodeViewLabel = gtk_label_new("Output");
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook2), textWidget, gCodeViewLabel);
	gtk_label_set_text(GTK_LABEL(OutputInfoLabel), output_info);

	gCodeViewLabelLua = gtk_label_new("PostProcessor");
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook2), textWidgetLuaBox, gCodeViewLabelLua);


//	hbox = gtk_hbox_new(0, 0);
//	gtk_box_pack_start(GTK_BOX(hbox), NbBox, 0, 0, 0);
//	gtk_box_pack_start(GTK_BOX(hbox), NbBox2, 0, 0, 0);

	hbox = gtk_hpaned_new();
	gtk_paned_pack1(GTK_PANED(hbox), NbBox, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(hbox), NbBox2, TRUE, TRUE);


	SizeInfoLabel = gtk_label_new("Width=0mm / Height=0mm");
	GtkWidget *SizeInfo = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(SizeInfo), SizeInfoLabel);
	gtk_container_set_border_width(GTK_CONTAINER(SizeInfo), 4);

	GtkWidget *LogoIMG = gtk_image_new_from_file("icons/logo-top.png");
	GtkWidget *Logo = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(Logo), LogoIMG);
	gtk_signal_connect(GTK_OBJECT(Logo), "button_press_event", GTK_SIGNAL_FUNC(handler_about), NULL);


	GtkWidget *topBox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(topBox), ToolBar, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(topBox), SizeInfo, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(topBox), Logo, 0, 0, 0);


	vbox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), MenuBar, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), topBox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(vbox), StatusBar, 0, 0, 0);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "CAMmill 2D");


	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_icon(GTK_WINDOW(window), create_pixbuf("icons/logo-top.png"));

	gtk_signal_connect(GTK_OBJECT(window), "destroy_event", GTK_SIGNAL_FUNC (handler_destroy), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC (handler_destroy), NULL);
	gtk_container_add (GTK_CONTAINER(window), vbox);

	gtk_widget_show_all(window);

#ifdef USE_POSTCAM
	strcpy(output_extension, "ngc");
	strcpy(output_info, "");
	postcam_init_lua(postcam_plugins[PARAMETER[P_H_POST].vint]);
	postcam_plugin = PARAMETER[P_H_POST].vint;
	gtk_label_set_text(GTK_LABEL(OutputInfoLabel), output_info);
	char tmp_str[1024];
	sprintf(tmp_str, "Output (%s)", output_extension);
	gtk_label_set_text(GTK_LABEL(gCodeViewLabel), tmp_str);
	postcam_load_source(postcam_plugins[PARAMETER[P_H_POST].vint]);
#endif

	gtk_timeout_add(1000/25, handler_periodic_action, NULL);
	gtk_main ();

#ifdef USE_POSTCAM
	postcam_exit_lua();
#endif

	return 0;
}


