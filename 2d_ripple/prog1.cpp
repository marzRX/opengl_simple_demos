#include <stdio.h>
#include <stdlib.h>
#if defined(GLEW)
#include <GL/glew.h>
#elif defined(GL3W)
#include <GL/gl3w.h>
#endif
#include <GLFW/glfw3.h>
#include "marTexture.h"
#include "marGLSLLite.h"
#include "marShader.h"
#include "marHRTimer.h"
#include <math.h>
#include <string>

enum {
  TEX_SRC=0,
  TEX_DST,
  TEX_MAX
};
TTextureInfo m_texture[TEX_MAX];
GLuint m_framebuffer;
int img_width, img_height;
int client_width, client_height;

enum {
  GLSL_RIPPLE,
  GLSL_NEGAPOSI,
  GLSL_UVMAP2D,
  GLSL_MAX
};
TShader shader[GLSL_MAX];

int running;

// GL_TRIANGLE_STRIP を使うので4頂点を用意
// 最初の三角形が反時計周りになるように
float w=1.0f;
GLfloat verts[] = {
  -w, -w, 0.0f,
  w, -w, 0.0f,
  -w, w, 0.0f,
  w, w, 0.0f,
};
GLfloat uvs[] = {
  0.0f, 1.0f, // (0, 1)
  1.0f, 1.0f, // (1, 1)
  0.0f, 0.0f, // (0, 0)
  1.0f, 0.0f, // (1, 0)
};

struct TBasicData {
  float img_size_x;
  float img_size_y;
  float dummy[2];
};
TBasicData basicdata;

struct TRippleCore {
  float touch_x;
  float touch_y;
  float amp;
  float delta_kappa;
//  float dummy[2]; // 配列は、16バイトアラインなので
};

struct TRippleSet {
  TRippleCore rip[2];
  int count;
};
TRippleSet rippleset;

enum {
  VAO_RIPPLE=0,
  VAO_UVMAP2D,
  VAO_MAX
};
GLuint vao[VAO_MAX];

GLuint vbo = 0;
GLuint vbo_uvs = 0;
const int VERTS_COUNT = 4;

GLuint ubo_basics;
GLuint ubo_ripple;

float totalframe = 0.0;
int framecounter = 0;
float g_amp = 16;
float g_delta_kappa = 0.0;

float img_pos_x = 256.0;
float img_pos_y = 256.0;

THighRezoTimer HRTimer;
double delta_theta = 0.0;
double theta_sum = 0.0;
float move_step;

// プロトタイプ宣言
void quit_opengl();

// VBO バッファを生成しデータを与える
unsigned int makeVBO(int datasize, float *data)
{
  GLuint vbo;

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, datasize, data, GL_STATIC_DRAW);

  return vbo;
}

// UBO バッファを生成しデータを与える
unsigned int makeUBO(int datasize, void *data)
{
  GLuint ubo;

  glGenBuffers(1, &ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, datasize, data, GL_DYNAMIC_DRAW);

  return ubo;
}

// initialization
void init(void)
{
  // GLSLプログラムを作成する
  MAR_GLSL_DEBUG = false;

  std::string FILE_SRC = "../images/blowfish.png";

  marCreateGLSLProgram(&shader[GLSL_RIPPLE].program, "vs4.glsl", "fs4_ripple.glsl");
  marCreateGLSLProgram(&shader[GLSL_UVMAP2D].program, "vs4_uvmap2d.glsl", "fs4_uvmapxd.glsl");

  //----------------------------------------------------------------------
  // 背景色を指定する
  glClearColor(0.3, 0.4, 0.7, 1.0);

  // 裏面を表示しない
  glEnable(GL_CULL_FACE);

  //----------------------------------------------------------------------
  // フレームバッファを作成
  glGenFramebuffers(1, &m_framebuffer);

  // PNG をロード
  m_texture[TEX_SRC] = loadTextureEx(FILE_SRC.c_str(), GL_LINEAR);
  if (m_texture[TEX_SRC].id == 0) {
    fprintf(stderr, "ERROR: could not load the image: %s\n", FILE_SRC.c_str());
    exit(EXIT_FAILURE);
  }
  img_width = m_texture[TEX_SRC].width;
  img_height = m_texture[TEX_SRC].height;

  // 空のテクスチャを作る
  m_texture[TEX_DST] = createEmptyTexture(img_width, img_height, GL_NEAREST);

  //----------------------------------------------------------------------
  // Location
  //----------------------------------------------------------------------
  shader[GLSL_RIPPLE].Enable();
  {
    shader[GLSL_RIPPLE].SetLocation("aPosition", 0);

    // ここで、マップの一覧表示とエラーの有無をチェック
    if (shader[GLSL_RIPPLE].PrintLocations() != 0) {
      printf("Shader Error: ロケーションの取得に失敗しました。\n");
      exit(EXIT_FAILURE);
    }
  }
  shader[GLSL_RIPPLE].Disable();

  shader[GLSL_UVMAP2D].Enable();
  {
    shader[GLSL_UVMAP2D].SetLocation("aPosition", 0);
    shader[GLSL_UVMAP2D].SetLocation("aUV", 1);

    if (shader[GLSL_UVMAP2D].PrintLocations() != 0) {
      printf("Shader Error: ロケーションの取得に失敗しました。\n");
      exit(EXIT_FAILURE);
    }
  }
  shader[GLSL_UVMAP2D].Disable();


  //----------------------------------------------------------------------
  // VAO
  //----------------------------------------------------------------------
  glGenVertexArrays(1, &vao[VAO_RIPPLE]);
  glGenVertexArrays(1, &vao[VAO_UVMAP2D]);

  //----------------------------------------------------------------------
  // VAO Binding
  //----------------------------------------------------------------------
  glBindVertexArray(vao[VAO_RIPPLE]);
  {
    // 頂点
    vbo = makeVBO(sizeof(verts), verts);
    glVertexAttribPointer(shader[GLSL_RIPPLE].GetLocation("aPosition"), 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_RIPPLE].GetLocation("aPosition"));
  }
  glBindVertexArray(0);

  glBindVertexArray(vao[VAO_UVMAP2D]);
  {
    // 頂点
    vbo = makeVBO(sizeof(verts), verts);
    glVertexAttribPointer(shader[GLSL_UVMAP2D].GetLocation("aPosition"), 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_UVMAP2D].GetLocation("aPosition"));

    vbo = makeVBO(sizeof(uvs), uvs);
    glVertexAttribPointer(shader[GLSL_UVMAP2D].GetLocation("aUV"), 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_UVMAP2D].GetLocation("aUV"));
  }
  glBindVertexArray(0);

  //----------------------------------------------------------------------
  // Uniform変数初期値
  basicdata.img_size_x = img_width;
  basicdata.img_size_y = img_height;
  rippleset.rip[0].touch_x = img_width/2.0-128.0*img_width/640.0;
  rippleset.rip[0].touch_y = img_height/2.0-128.0*img_width/640.0;
  rippleset.rip[1].touch_x = img_width/2.0+128.0*img_width/640.0;
  rippleset.rip[1].touch_y = img_height/2.0+128.0*img_width/640.0;
  rippleset.count = 2;

  ubo_basics = makeUBO(sizeof(basicdata), &basicdata);
  ubo_ripple = makeUBO(sizeof(rippleset), &rippleset);
}

/*
  program: シェーダプログラム
  name: uniform で glsl プログラムに渡す名前
  unit_no: テクスチャーユニットの番号
  texture: m_texture[]
*/
void bind_sampler(GLuint program_id, const char *name, GLint texture_unitno, GLuint texture)
{
  // テクスチャユニットとシェーダのバインディング番号を合わせる。名前引きはしない。
  // 名前引きで動くのは、NVIDIA だけ。AMDでは動かない。
//  GLuint location = glGetUniformLocation(program_id, name);
//  glUniform1i(location, texture_unitno);
  glActiveTexture(GL_TEXTURE0 + texture_unitno);
  glBindTexture(GL_TEXTURE_2D, texture);
}

//void window_refresh_callback(GLFWwindow* window)
void paintGL()
{
  framecounter++;

  // g_amp が小さくなるほど、小さくする割合を小さくしたい
  double k=g_amp;
  if (k>1.0) {
    k=1.0;
  }
  k = pow(k, 10.0);
  g_amp -= delta_theta * pow(1.1, k);

#ifdef VERBOSE
  printf("g_amp = %.6f, framecounter = %d\n", g_amp, framecounter);
#endif
  if (g_amp<=0) {
    g_amp = 0.0;
  }

  // glClear() で指定した背景色で塗りつぶす
  glClear(GL_COLOR_BUFFER_BIT);

  int dstWidth, dstHeight;
  dstWidth = img_width;
  dstHeight = img_height;

  glViewport(0, 0, dstWidth, dstHeight); // 新たな glViewport を用意

  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
  {
    // 出力を受けるテクスチャーを指定する
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_texture[TEX_DST].id, 0);
    // シェーダプログラムの指定
    shader[GLSL_RIPPLE].Enable();
    {
      glBindVertexArray(vao[VAO_RIPPLE]);
      {
        bind_sampler(shader[GLSL_RIPPLE].program, "image", 0, m_texture[TEX_SRC].id); // テクスチャユニット0 に、テクスチャ image を渡す

        rippleset.rip[0].touch_x = img_pos_x;
        rippleset.rip[0].touch_y = img_pos_y;

        rippleset.rip[0].amp = g_amp;
        rippleset.rip[0].delta_kappa += delta_theta;

        rippleset.rip[1].amp = g_amp;
        rippleset.rip[1].delta_kappa += delta_theta;

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_ripple);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TRippleSet), &rippleset);

        // Uniform Block 対応コード
        GLuint bindingPoint;
        bindingPoint = 0;
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo_basics);
        bindingPoint = 1;
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo_ripple);

        // drawcall
        glDrawArrays(GL_TRIANGLE_STRIP, 0, VERTS_COUNT);
      }
      glBindVertexArray(0);
    }
    shader[GLSL_RIPPLE].Disable();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);



  // 表示用シェーダで表示
  glViewport(0, 0, client_width, client_height);
  shader[GLSL_UVMAP2D].Enable();
  {
    glBindVertexArray(vao[VAO_UVMAP2D]);
    {
      bind_sampler(shader[GLSL_UVMAP2D].program, "image", 0, m_texture[TEX_DST].id); // テクスチャユニット0 に、テクスチャ image を渡す
      // drawcall
      glDrawArrays(GL_TRIANGLE_STRIP, 0, VERTS_COUNT);
    }
    glBindVertexArray(0);
  }
  shader[GLSL_UVMAP2D].Disable();

//  glfwSwapBuffers(window);
}

void window_size_callback(GLFWwindow* window, int width, int height)
{
  client_width = width;
  client_height = height;

  glViewport(0, 0, width, height);
}

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
  switch(codepoint) {
  case 'q':
  case 'Q':
    running = GL_FALSE;
    break;

  default:
    break;
  }
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
  if ((button == GLFW_MOUSE_BUTTON_LEFT) && (action == GLFW_PRESS)) {
    g_amp = 16;
    framecounter = 0;

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    // ウィンドウ座標を画像ピクセル座標へ変換する(リサイズしてもずれない)
    img_pos_x = x * img_width  / client_width;
    img_pos_y = y * img_height / client_height;
  }
}

void animate()
{
  totalframe += 1.0;

  // アニメーション関係のパラメータをここで計算する
  // framecountのカウントアップや、移動や回転のパラメータの計算もここで行う

  double delta_time = HRTimer.getELT();
  //  printf("delta_time = %.2f\n", delta_time);

  const float SPEED_CTL = 0.8;

  // １秒で 2PI 動いてね
  delta_theta = 2.0*M_PI * delta_time;
  // theta のトータル
  theta_sum += delta_theta;

  // 時間をコントロールする係数
  move_step = (sin(theta_sum * SPEED_CTL)+1)/2.0; // move_step = range[0.0 .. 1.0]
  move_step = 0.5 * move_step + 0.4; // move_step = range[0.4 .. 0.9]
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

  // 画像のサイズが決まるまでウィンドウを隠しておく
  // (表示してからサイズを変えると、ウィンドウマネージャーが位置を画面の端へ寄せてしまう)
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  client_width = 512;
  client_height = 512;
  GLFWwindow* window = glfwCreateWindow(client_width, client_height, "Ripple", NULL, NULL);
  if (! window) {
    fprintf(stderr, "ERROR: could not open window with GLFW3\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

#ifdef GPUINFO
  // glutCreateWindow() の後に、GPU情報を取得する。
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
  init();

  // ウィンドウは幅 1024 を基準に、画像のアスペクト比に合わせる
  client_width = 1024;
  client_height = 1024 * img_height / img_width;
  glfwSetWindowSize(window, client_width, client_height);
  glfwShowWindow(window);

  // callback
  // 描画処理でシェーダを使うのでシェーダコンパイル後にコールバックの設定をする
  glfwSetWindowSizeCallback(window, window_size_callback);
//  glfwSetWindowRefreshCallback(window, window_refresh_callback);
  glfwSetCharCallback(window, char_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);

  // アニメーション対応コード
  // 参考HP
  // https://tatsy.github.io/OpenGLCourseJP/sections/animation/one_cube.html

  running = GL_TRUE;
  while (running && (! glfwWindowShouldClose (window)) ) {

    paintGL();

    animate();

    glfwSwapBuffers(window);
    glfwPollEvents();

    //ESCキーで終了
    running = !glfwGetKey(window, GLFW_KEY_ESCAPE ) ;
//    glfwWaitEvents();
  }

  quit_opengl();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

// 終了処理
void quit_opengl()
{
  // フレームバッファの削除
  glDeleteFramebuffers(1, &m_framebuffer);

  // テクスチャの削除
  for (int i=0; i<TEX_MAX; i++) {
    glDeleteTextures(1, &m_texture[i].id);
  }

  // UBO / VAO の削除
  glDeleteBuffers(1, &ubo_basics);
  glDeleteBuffers(1, &ubo_ripple);
  glDeleteVertexArrays(VAO_MAX, vao);

  // GLSLプログラムの削除
  for (int i=0; i<GLSL_MAX; i++) {
    glDeleteProgram(shader[i].program);
  }
}
