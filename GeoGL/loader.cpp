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


#define _CRT_SECURE_NO_DEPRECATE // Ref: https://stackoverflow.com/questions/14386/fopen-deprecated-warning
#include <direct.h> // For _getcwd: https://msdn.microsoft.com/en-us/library/sf98bd4y.aspx


#include <SDL2/SDL_image.h>
#include <boost/asio/io_service.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <curl/curl.h>
#include <sstream>

#include "global.h"
#include "loader.h"

boost::thread_group pool;
boost::asio::io_service ioService;
boost::asio::io_service::work *work;

Loader *Loader::_instance = nullptr;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t written;
  written = fwrite(ptr, size, nmemb, stream);
  return written;
}

Loader::Loader() {
  work = new boost::asio::io_service::work(ioService);
  for (int i = 0; i < 5; i++) {
    pool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
  }
}

Loader::~Loader() {
  ioService.stop();
  pool.join_all();
  delete work;
}

void Loader::download_image(Tile *tile) {

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    std::cerr << "Failed to initialize curl" << std::endl;
    return;
  }

  std::stringstream dirname;
  dirname << TILE_DIR << tile->zoom << "/" << tile->x;
  std::string dir = dirname.str();
  boost::filesystem::create_directories(dir);
  std::string filename = tile->get_filename();
  //std::string url = "http://127.0.0.1:8080/styles/dark-matter/" + filename;
  std::string url = "http://a.tile.openstreetmap.org/" + filename;
  std::string file = TILE_DIR + filename;
  FILE *fp = fopen(file.c_str(), "wb");
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  CURLcode res = curl_easy_perform(curl);
  fclose(fp);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    std::cerr << "Failed to download: " << url << " " << res << std::endl;
  } else {
    tile->texid = 0;
  }
}

void Loader::load_image(Tile &tile) {
  std::string filename = TILE_DIR + tile.get_filename();
  if (!boost::filesystem::exists(filename)) {
    ioService.post(boost::bind(&Loader::download_image, this, &tile));
    return;
  }
  if (boost::filesystem::file_size(filename) == 0) {
    boost::filesystem::remove(filename);
    ioService.post(boost::bind(&Loader::download_image, this, &tile));
    return;
  }

  open_image(tile);
}

void Loader::open_image(Tile &tile) {
  std::string filename = TILE_DIR + tile.get_filename();
  SDL_Surface *texture = IMG_Load(filename.c_str());

  char tmp[4096];
  _getcwd(tmp, 4096);
  std::cout << "Loading texture " << filename << " from directory " << tmp
            << ' ';

  if (texture) {
    if (SDL_MUSTLOCK(texture)) {
      SDL_LockSurface(texture);
    }

	// GL_BGR to GL_BGR_EXT: https://www.opengl.org/discussion_boards/showthread.php/122861-GL_BGR

    GLenum texture_format;
    if (texture->format->BytesPerPixel == 4) {
      if (texture->format->Rmask == 0x000000ff) {
        texture_format = GL_RGBA;
      } else {
        texture_format = GL_BGRA_EXT;
      }
    } else if (texture->format->BytesPerPixel == 3) {
      if (texture->format->Rmask == 0x000000ff) {
        texture_format = GL_RGB;
      } else {
        texture_format = GL_BGR_EXT;
      }
    } else {
      std::cout << "INVALID ("
                << SDL_GetPixelFormatName(texture->format->format);
      SDL_PixelFormat *format = SDL_AllocFormat(SDL_PIXELFORMAT_BGR24);
      SDL_Surface *tmp = SDL_ConvertSurface(texture, format, 0);
      texture_format = GL_BGR_EXT;
      if (SDL_MUSTLOCK(texture)) {
        SDL_UnlockSurface(texture);
      }
      SDL_FreeSurface(texture);
      texture = tmp;
      if (SDL_MUSTLOCK(texture)) {
        SDL_LockSurface(texture);
      }
      std::cout << " -> " << SDL_GetPixelFormatName(texture->format->format)
                << ") ";
    }

    GLuint texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);

    glTexImage2D(GL_TEXTURE_2D, 0, 3, texture->w, texture->h, 0, texture_format,
                 GL_UNSIGNED_BYTE, texture->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (SDL_MUSTLOCK(texture)) {
      SDL_UnlockSurface(texture);
    }
    SDL_FreeSurface(texture);

    tile.texid = texid;

    std::cout << "SUCCESS" << std::endl;
  } else {
    tile.texid = TileFactory::instance()->get_dummy();
    std::cout << "FAILED" << std::endl;
  }
}
