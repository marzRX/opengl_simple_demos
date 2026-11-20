#include <stdio.h>
#if defined(GLEW)
#include <GL/glew.h>
#elif defined(GL3W)
#include <GL/gl3w.h>
#endif
#include <GLFW/glfw3.h>
#include "marTexture.h"
#include "marGLSLLite.h"
#include "marShader.h"

enum {
  TEX_SRC=0,
  TEX_DST,  // DFTの出力 (実部・虚部)
  TEX_RESTORE, // IDFTの出力
  TEX_MAX
};
TTextureInfo m_texture[TEX_MAX];
GLuint m_framebuffer;

// 1次元のデータ
const int datasize_x = 8;
const int datasize_y = 1;
// GL_TEXTURE_1Dの実装はドライバに差があると思われるので、GL_TEXTURE_2Dで１次元のデータを作る

enum {
  GLSL_CALC_PROC, // 単一シェーダによる計算処理のときは、GLSL_CALC_PROC を使用する(毎回リネームするのは大変だから）
  GLSL_IDFT,
  GLSL_MAX
};
TShader shader[GLSL_MAX];

// GL_TRIANGLE_STRIP を使うので4頂点を用意
// 最初の三角形が反時計周りになるように
float w=1.0f;
GLfloat verts[] = {
  -w, -w, 0.0f,
  w, -w, 0.0f,
  -w, w, 0.0f,
  w, w, 0.0f,
};

enum {
  VAO_CALC_PROC,
  VAO_MAX
};
GLuint vao[VAO_MAX];
GLuint vbo_verts;

const int VERTS_COUNT4 = 4;

int client_width = 640;
int client_height = 640;

// プロトタイプ宣言
void quit_opengl();

//----------------------------------------------------------------------
// MY DATA DESCRIPTION SETUP
//----------------------------------------------------------------------
const int SRC_GPU_FORMAT = GL_RG32F; // OpenGL Internal FORMAT(gpuサイド)
const int SRC_APP_FORMAT = GL_RG;   // R=実部, G=虚部
const int SRC_APP_DATATYPE = GL_FLOAT;

// 入力データ: 実部1から8(虚部は0)
GLfloat floatArray[datasize_x * 2] = {
  1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f, 4.0f, 0.0f,
  5.0f, 0.0f, 6.0f, 0.0f, 7.0f, 0.0f, 8.0f, 0.0f
};

void print_complex_texture(const char *title, GLuint texture)
{
  GLfloat data[datasize_x * 2] = {};
  glBindTexture(GL_TEXTURE_2D, texture);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, data);
  printf("------------------------------\n");
  printf("%s\n", title);
  for (int i=0; i<datasize_x; i++) {
    printf("%d: % .6f  % .6f\n", i, data[i*2], data[i*2+1]);
  }
}


// VBO バッファを生成しデータを与える
unsigned int makeVBO(int datasize, float *data)
{
  GLuint vbo;

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, datasize, data, GL_STATIC_DRAW);

  return vbo;
}

// initialization
int init()
{
  MAR_GLSL_DEBUG = false;

  // GLSLプログラムを作成する
  marCreateGLSLProgram(&shader[GLSL_CALC_PROC].program, "vs4_calcproc.glsl", "fs4_dft.glsl");
  marCreateGLSLProgram(&shader[GLSL_IDFT].program, "vs4_calcproc.glsl", "fs4_idft.glsl");

  // フレームバッファを作成
  glGenFramebuffers(1, &m_framebuffer);

  // 入力・DFT・IDFT結果のテクスチャを作る
  m_texture[TEX_SRC] = createEmptyTextureAC(datasize_x, datasize_y,
                                            TGLFormat::RG32F, GL_NEAREST);
  m_texture[TEX_DST] = createEmptyTextureAC(datasize_x, datasize_y,
                                            TGLFormat::RG32F, GL_NEAREST);
  m_texture[TEX_RESTORE] = createEmptyTextureAC(datasize_x, datasize_y,
                                                TGLFormat::RG32F, GL_NEAREST);

  // データを作成してテクスチャに設定
  glBindTexture(GL_TEXTURE_2D, m_texture[TEX_SRC].id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, datasize_x, datasize_y,
                  SRC_APP_FORMAT, SRC_APP_DATATYPE, floatArray);
  glBindTexture(GL_TEXTURE_2D, 0);

  print_complex_texture("入力データ", m_texture[TEX_SRC].id);

  //----------------------------------------------------------------------
  shader[GLSL_CALC_PROC].Enable();
  {
    // シェーダからロケーションの取得
    shader[GLSL_CALC_PROC].SetLocation("aPosition", 0);

    if (shader[GLSL_CALC_PROC].PrintLocations() != 0) {
      printf("Shader Error: ロケーションの取得に失敗しました。\n");
      return -1;
    }
  }
  shader[GLSL_CALC_PROC].Disable();

  //----------------------------------------------------------------------
  // VAO
  //----------------------------------------------------------------------
  glGenVertexArrays(1, &vao[VAO_CALC_PROC]);

  //----------------------------------------------------------------------
  // VAO Binding
  //----------------------------------------------------------------------
  glBindVertexArray(vao[VAO_CALC_PROC]);
  {
    vbo_verts = makeVBO(sizeof(verts), verts);
    glVertexAttribPointer(shader[GLSL_CALC_PROC]["aPosition"], 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(shader[GLSL_CALC_PROC]["aPosition"]);
  }
  glBindVertexArray(0);

  return 0;
}

/*
  program: シェーダプログラム
  name: uniform で glsl プログラムに渡す名前
  unit_no: テクスチャーユニットの番号
  texture: テクスチャのID
*/
void bind_sampler(GLuint program, const char *name, GLint unit_no, GLuint texture)
{
  glUniform1i(glGetUniformLocation(program, name), unit_no);
  glActiveTexture(GL_TEXTURE0 + unit_no); // テクスチャユニット unit_no
  glBindTexture(GL_TEXTURE_2D, texture);
}

// DFT→IDFT の順に2パス実行する
void calc_proc()
{
  glViewport(0, 0, datasize_x, datasize_y);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_framebuffer);
  glBindVertexArray(vao[VAO_CALC_PROC]);

  for (int i=0; i<2; i++) {
    const int shader_no = (i == 0) ? GLSL_CALC_PROC : GLSL_IDFT;
    const int src_no = (i == 0) ? TEX_SRC : TEX_DST;
    const int dst_no = (i == 0) ? TEX_DST : TEX_RESTORE;

    // 入力テクスチャと出力テクスチャは別々にする
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_texture[dst_no].id, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Framebuffer incomplete (pass=%d)\n", i);
      break;
    }

    shader[shader_no].Enable();
    bind_sampler(shader[shader_no].program, "src", 0, m_texture[src_no].id);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, VERTS_COUNT4);
    shader[shader_no].Disable();
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

int main()
{
  if (! glfwInit() ) {
    fprintf(stderr, "ERROR: could not start GLFW3\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // higher 3.2

  // window を unvisible に
  glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

  GLFWwindow *window = glfwCreateWindow(client_width, client_height,
					"DFT", NULL, NULL);
  if (! window) {
    fprintf(stderr, "ERROR: could not open window with GLFW3\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

#ifdef GPUINFO
  // gl**CreateWindow() の後に、GPU情報を取得する。
  printf("OpenGL Vendor  = %s\n", glGetString(GL_VENDOR));
  printf("OpenGL GPU     = %s\n", glGetString(GL_RENDERER));
  printf("OpenGL Version = %s\n", glGetString(GL_VERSION));
  printf("GLSL   Version = %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
#endif

  // OpenGL 拡張機能を使えるように！！
#if defined(GLEW)
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    printf("Error: %s\n", glewGetErrorString(err));
    return 1;
  }
#elif defined(GL3W)
  if (gl3wInit()) {
    fprintf(stderr, "failed to initialize OpenGL\n");
    return 1;
  }
  if (!gl3wIsSupported(4, 5)) {
    fprintf(stderr, "OpenGL 4.5 not supported\n");
    return 1;
  }
#endif
  printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
         glGetString(GL_SHADING_LANGUAGE_VERSION));

  // 拡張命令を有効にしてから初期化処理を呼ぶ
  if (init() != 0) {
    fprintf(stderr, "Initialization failed\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  // GPUによる計算
  calc_proc();

  print_complex_texture("DFT 変換データ", m_texture[TEX_DST].id);
  print_complex_texture("IDFT 復元データ", m_texture[TEX_RESTORE].id);

  quit_opengl();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

// 終了処理
void quit_opengl()
{
  // VAO / VBO の削除
  glDeleteVertexArrays(VAO_MAX, vao);
  glDeleteBuffers(1, &vbo_verts);

  // フレームバッファの削除
  glDeleteFramebuffers(1, &m_framebuffer);

  // テクスチャの削除
  for (int i=0; i<TEX_MAX; i++) {
    glDeleteTextures(1, &m_texture[i].id);
  }

  // GLSLプログラムの削除
  for (int i=0; i<GLSL_MAX; i++) {
    glDeleteProgram(shader[i].program);
  }

  // glDisableVertexAttribArray()は marShaderの中で処理しているので不要
}
