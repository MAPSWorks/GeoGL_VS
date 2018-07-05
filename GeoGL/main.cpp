/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Christoph Brill
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#define NOMINMAX // Ref: https://stackoverflow.com/questions/7035023/stdmax-expected-an-identifier
#define SDL_MAIN_HANDLED // Ref: https://www.opengl.org/discussion_boards/showthread.php/200005-LNK1561-entry-point-must-be-defined

#include <iostream>
#include <thread>
#include <algorithm>
#include <cmath>

//#include <unistd.h>
#include <Windows.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GLUT/glut.h>

#include "tile.h"
#include "loader.h"
#include "input.h"
#include "global.h"
#include "dots.h"
#include "time.h"


/**
 * @brief poll for events
 * @return false, if the program should end, otherwise true
 */
bool poll() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    return false;
                }
                break;
            case SDL_MOUSEMOTION:
                handle_mouse_motion(event.motion);
                break;
            case SDL_MOUSEBUTTONDOWN:
                handle_mouse_button_down(event.button);
                break;
            case SDL_MOUSEBUTTONUP:
                handle_mouse_button_up(event.button);
                break;
            case SDL_MOUSEWHEEL:
                handle_mouse_wheel(event.wheel);
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        window_state.width = event.window.data1;
                        window_state.height = event.window.data2;
                        glViewport(0, 0, window_state.width, window_state.height);
                        break;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

void draw_circle(float cx, float cy, float r, int num_segments)
{
	float theta = 2 * 3.1415926 / float(num_segments);
	float c = cosf(theta);//precalculate the sine and cosine
	float s = sinf(theta);
	float t;

	float x = r;//we start at angle = 0
	float y = 0;

	glBegin(GL_POLYGON);
	for(int ii = 0; ii < num_segments; ii++)
	{
		glVertex2f(x + cx, y + cy);//output vertex

		//apply the rotation matrix
		t = x;
		x = c * x - s * y;
		y = s * t + c * y;
	}
	glEnd();
}

void draw_text(std::string string, int x, int y, int z)
{
    const char *c;
    glRasterPos3f(x, y,z);

    for (c = string.c_str(); *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18 , *c);
    }
}

void render(int zoom, double latitude, double longitude, Dots* dots) {
    Tile* center_tile = TileFactory::instance()->get_tile(zoom, latitude, longitude);

    // Clear with black
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE_ARB);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-(window_state.width / 2), (window_state.width / 2), (window_state.height / 2), -(window_state.height / 2), -1000, 1000);

    glPushMatrix();
        // Rotate and and tilt the world geometry
        glRotated(viewport_state.angle_tilt, 1.0, 0.0, 0.0);
        glRotated(viewport_state.angle_rotate, 0.0, 0.0, -1.0);

        int map_size = mapsize(zoom);

        int center_tile_x = long2x(tilex2long(center_tile->x, zoom), map_size);
        int center_tile_y = lat2y(tiley2lat(center_tile->y, zoom), map_size);
        int ref_x = long2x(longitude, map_size);
        int ref_y = lat2y(latitude, map_size);

        glTranslated(center_tile_x - ref_x, center_tile_y - ref_y, 0);

        // Render the slippy map parts
        glEnable(GL_TEXTURE_2D);

            glPushMatrix();

                static const int top = -4;
                static const int left = -4;
                static const int bottom = 5;
                static const int right = 5;

                // Start 'left' and 'top' tiles from the center tile and render down to 'bottom' and
                // 'right' tiles from the center tile
                Tile* current = center_tile->get(left, top);
                for (int y = top; y < bottom; y++) {
                    for (int x = left; x < right; x++) {

                        // If the texid is set to zero the download was finished successfully and
                        // the tile can be rendered now properly
                        if (current->texid == 0) {
                            Loader::instance()->open_image(*current);
                        }

                        // Render the tile itself at the correct position
                        glPushMatrix();
                            glTranslated(x*128*2, y*128*2, 0);
                            glBindTexture(GL_TEXTURE_2D, current->texid);
                            glBegin(GL_QUADS);
                                glTexCoord2f(0.0, 1.0); glVertex3f(0, 256, 0);
                                glTexCoord2f(1.0, 1.0); glVertex3f(256, 256, 0);
                                glTexCoord2f(1.0, 0.0); glVertex3f(256, 0, 0);
                                glTexCoord2f(0.0, 0.0); glVertex3f(0, 0, 0);
                            glEnd();
                        glPopMatrix();
                        current = current->get_west();
                    }
                    current = current->get(-(std::abs(left) + std::abs(right)), 1);
                }
            glPopMatrix();
        glDisable(GL_TEXTURE_2D);

        glColor4d(1.0, 193.0 / 255.0, 7.0 / 255.0, 0.3);

        double base_size = pow(2, zoom) / 50.0;

        int min_x = -(window_state.width);
        int max_x =  (window_state.width);
        int min_y = -(window_state.height);
        int max_y =  (window_state.height);

        uint64_t time = get_time();
        uint64_t msec = 1000000;

        for (auto const &di : dots->dots) {
            Dot dot = di.second;
            int x = dot.zoom_x[zoom] - center_tile_x;
            int y = dot.zoom_y[zoom] - center_tile_y;

            if (x < min_x || x > max_x || y < min_y || y > max_y) {
                continue;
            }

            uint64_t time_diff = dots->time_diff(dot.seen_at, time);

            double pulse_period = 1000 * msec / M_PI;
            double pulse = (sin(time_diff / pulse_period) + 1) / 6;

            double alpha = pulse + 0.2;
            double size = std::max(2.0, log2(base_size * dot.size));

            glColor4d(1.0, 193.0 / 255.0, 7.0 / 255.0, alpha);
            draw_circle(x, y, size, std::max(20, (int)(size * 10)));
        }
        glColor3d(1.0, 1.0, 1.0);
    glPopMatrix();

    glPushMatrix();
        glTranslated(-(window_state.width / 2), -(window_state.height / 2), 0);
        draw_text("Live " + std::to_string(dots->dots.size()), 20, 30, 0);
    glPopMatrix();
}

int main(int argc, char *argv[]) {

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Could not initialize SDL video: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create an OpenGL window
    SDL_Window* window = SDL_CreateWindow("slippymap3d", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Could not create SDL window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_GetWindowSize(window, &window_state.width, &window_state.height);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    glutInit(&argc, argv);
    glutInitWindowSize(640,480);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

    Dots* dot_manager = new Dots();
    dot_manager->run();

    while(true) {
        if (!poll()) {
            break;
        }
        render(player_state.zoom, player_state.latitude, player_state.longitude, dot_manager);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

