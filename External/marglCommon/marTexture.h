#pragma once

#ifdef GLEW
#include <GL/glew.h>
#elif GL3W
#include <GL/gl3w.h>
#endif

struct TTextureInfo {
    GLuint id;
    GLsizei width;
    GLsizei height;
};

// テクスチャのフォーマット指定(createEmptyTextureAC で使用)
// 並びは gl_common2 の marTexture.h と同じ。値を変えないこと
enum TGLFormat {
  RGBA = 0,
  RGBA16F,
  RGBA32F,
  RED,
  R16F,
  R16F_UB,
  R16F_HF,
  R32F,
  RG32F, // 2チャネルfloat。DCT/DFTなどの複素数(R=実部, G=虚部)用
};

TTextureInfo createEmptyTexture(int width, int height, GLenum interp_mode=GL_NEAREST);

// AC: Auto Config
// TGLFormat から GPUFormat, EXTFormat, dataType を求めて空のテクスチャを作る
// createEmptyTexture() が RGBA 固定なのに対し、浮動小数点などのフォーマットも作れる
TTextureInfo createEmptyTextureAC(int width, int height, TGLFormat format=RGBA, GLenum interp_mode=GL_LINEAR); // default GL_LINEAR

TTextureInfo loadTextureEx(const char *filepath, GLenum interp_mode=GL_NEAREST);

bool saveFramebufferAsPNG(const char* filepath, int width, int height, GLuint framebuffer);
bool saveTextureAsPNG(const char* filepath, int width, int height, GLuint texture);
bool saveTexture(const char* filepath, int width, int height, GLuint texture);
bool saveScreen(const char* filepath, int screen_width, int screen_height);
