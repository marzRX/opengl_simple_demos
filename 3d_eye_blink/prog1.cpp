#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(GLEW)
#include <GL/glew.h>
#elif defined(GL3W)
#include <GL/gl3w.h>
#endif
#include <GLFW/glfw3.h>
#include <math.h>
#include <string>
#include "marGLSLLite.h"
#include "marMatrix2.h"
#include "marHRTimer.h"
#include "abaum.h"
#include "marShader.h"
#include "marQuatCamera.h"
#include "marTexture.h"

const char *EYEBALL_PNG = "eyeball_ipolar_pad_after_ipolar3.png";// 白目ノイズ解消版
const char *CHECKER_PNG = "uvchecker_earth_orange.png";

GLFWwindow* window;
int running;

enum {
  GLSL_SOLID_COLOR=0,
  GLSL_SOLID_COLOR2,
  GLSL_SHADING_TEX,
  GLSL_MAX,
};
TShader shader[GLSL_MAX];

enum {
  TEX_SRC=0,
  TEX_IMAGE_PROC,
  TEX_DST,
  TEX_CHECKER,
  TEX_MAX
};
TTextureInfo m_texture[TEX_MAX];

// まぶたのブロックにテクスチャを貼るか(キー T で切り替え)
bool gUseTexture = true;
int img_width, img_height;

int gIndex = 1;

// カメラ: 方位角 0度、仰角 0度(ラジアンで与える)、注視点との距離 5
TQuatCamera camera = TQuatCamera(0.0f, 0.0f, 5.0f);

// ズーム操作の起点となる距離(マウスを押した時点の距離)
float distance100 = 10.0f;

smat4 m_proj;
smat4 m_view;

//----------------------------------------------------------------------
bool FMouseDown = false;
bool FMouseZoomDown = false;
int FSX, FSY;
// スクリーンサイズに依存しないパンの実現のため
int client_width, client_height;

enum {
  VAO_IMAGE_PROC,
  VAO_MRT_RESULT,
  VAO_DRAW_MESH_TEX,
  VAO_EYELID_CENTER,
  VAO_EYELID_TWIN,
  VAO_MAX
};
GLuint vao[VAO_MAX];

GLfloat colors[] = {
  1.0f, 1.0f, 0.0f, // Yellow
  1.0f, 1.0f, 0.0f, // Yellow
  1.0f, 1.0f, 0.0f, // Yellow
  1.0f, 1.0f, 0.0f, // Yellow
  0.0f, 1.0f, 0.0f, // G
  0.0f, 1.0f, 0.0f, // G
  0.0f, 1.0f, 0.0f, // G
  0.0f, 1.0f, 0.0f, // G
  1.0f, 0.0f, 0.0f, // R
  1.0f, 0.0f, 0.0f, // R
  1.0f, 0.0f, 0.0f, // R
  1.0f, 0.0f, 0.0f, // R
  0.0f, 0.0f, 1.0f, // B
  0.0f, 0.0f, 1.0f, // B
  0.0f, 0.0f, 1.0f, // B
  0.0f, 0.0f, 1.0f, // B
  0.0f, 1.0f, 1.0f, // C
  0.0f, 1.0f, 1.0f, // C
  0.0f, 1.0f, 1.0f, // C
  0.0f, 1.0f, 1.0f, // C
  1.0f, 0.0f, 1.0f, // P
  1.0f, 0.0f, 1.0f, // P
  1.0f, 0.0f, 1.0f, // P
  1.0f, 0.0f, 1.0f, // P
};

// ブロックの UV。6面 x 4頂点。1面に、テクスチャ1枚が貼られる。
//   各面の頂点は、(v0,v1,v2) が表から見て反時計回りになる並びになっている。
//   PNG は上の行から GPU へ載るので、t は画像の上から下へ増える(t=0 が画像の上)。
//   そのため UV は、同じ並びを (u,t) 平面で時計回りにする (0,0)(0,1)(1,0)(1,1) とする。
//   これで、どの面でも鏡像にならない。外側の面では、画像が正立して貼られる。
GLfloat baum_uv[24][2] = {
  {0,0},{0,1},{1,0},{1,1},
  {0,0},{0,1},{1,0},{1,1},
  {0,0},{0,1},{1,0},{1,1},
  {0,0},{0,1},{1,0},{1,1},
  {0,0},{0,1},{1,0},{1,1},
  {0,0},{0,1},{1,0},{1,1},
};

THighRezoTimer HRTimer;
double theta_sum = 0.0;
float move_step;

// プロトタイプ宣言
void quit_opengl();

const int VCNT=65536;
float model_verts[VCNT][3]; // ポリゴン用の頂点データ
float model_norms[VCNT][3]; // ポリゴン用の法線データ
float model_uv[VCNT][2];
unsigned int model_index[VCNT][3];
int model_polygon_cnt;

GLuint vbo_baum[152] = {0};
GLuint vbo_baum_normal[152] = {0};
GLuint vbo_colors = 0;
GLuint vbo_baum_uv = 0;
GLuint ibo_baum = 0;

//GLint a_pos;
//GLint a_normal;
//GLint a_color;

GLuint loc_MVP; // loc_ModelViewProjection
GLuint loc_ModelView;
GLuint loc_View;
GLuint loc_NormalMatrix;

TABaum mBaum[152];

void baum(int N, int iLevel, int y, int z)
{
  int index = get_baum_index(iLevel, y, z);
  mBaum[index].setup_vertex(N, iLevel, y, z);
  mBaum[index].setup_normal(N, iLevel, y, z);
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

// 球はポリゴン数が大きくなるので配列のサイズに注意
// Sゴードン本を参考に
int uvsphere_sc(int prec, double radius)
{
  // 多面体を形成する、すべての頂点と法線
  float verts_skel[4096][3]; // 重複のない頂点セット
  //  float norms_skel[256][3]; // 重複のない法線セット
  // 球体は頂点座標がそのまま法線ベクトルとなる
  float st_skel[4096][2];

  double r = radius;

  int segment = prec;
  int ring = prec;
  // マージン付きの
  int segment_inc = segment+1;
  int ring_inc = ring+1;

  double phi;
  double theta, delta_theta;
  delta_theta = - 2.0*M_PI/segment;

  //  ring_inc = 3;
  int ixy;
  for (int y=0; y<ring_inc; y++) {
    //    phi = M_PI * (1.0 - (double)y/ring);
    phi = M_PI * (double)y/ring;
    for (int x=0; x<segment_inc; x++) {
      theta = delta_theta * x;
      ixy = x+y*segment_inc;

      verts_skel[ixy][0] = (float)(r * sin(phi) * cos(theta));
      verts_skel[ixy][1] = r * cos(phi);
      verts_skel[ixy][2] = (float)(r * sin(phi) * sin(theta));

      st_skel[ixy][0] = (float)x/prec;
      st_skel[ixy][1] = (float)y/prec;

//      st_skel[ixy][0] *= 3.0;
      st_skel[ixy][1] *= 5.0;
      if (st_skel[ixy][1] > 1.0)
        st_skel[ixy][1] = 1.0;

/*
      float tmp = (float)y/prec;
      tmp += 0.5;
      if (tmp >= 1.0) {
        tmp -= 1.0;
      }
      st_skel[ixy][1] = tmp;
*/
    }
  }

  int box_count = segment * ring;
  //  printf("box_count = %d\n", box_count);
  /*
  int ref[]={0,1,5,5,1,6, => 0,5,1,1,5,6 に修正
	     1,2,6,6,2,7,
	     2,3,7,7,3,8,
	     3,4,8,8,4,9,
	     5,6,10,10,6,11,
	     6,7,11,11,7,12,
	     7,8,12,12,8,13,
	     8,9,13,13,9,14,
  };
  */
  int ref[box_count*6];
  for (int y=0; y<ring; y++) {
    for (int x=0; x<segment; x++) {
      int ii=(x+y*segment)*6;
      //      printf("ii = %d\n", ii);
      ref[ii+0] = x+y*segment_inc;
      ref[ii+1] = x+(y+1)*segment_inc;
      ref[ii+2] = (x+1)+y*segment_inc;
      
      ref[ii+3] = (x+1)+y*segment_inc;
      ref[ii+4] = x+(y+1)*segment_inc;
      ref[ii+5] = (x+1)+(y+1)*segment_inc;
    }
  }

  for (int i=0; i<box_count*2*3; i++) {
    //    printf("ref[%d]=%d\n", i, ref[i]);
  }

  //  int verts_count = segment_inc * ring_inc;
  int polygon_cnt = box_count * 2;
  //  printf("polygon_cnt = %d\n", polygon_cnt);
  for (int i=0; i<polygon_cnt*3; i++) {
    int ii = ref[i];
    if (i % 3 == 0) {
      //      printf("\n");
    }
    model_verts[i][0] = verts_skel[ii][0];
    model_verts[i][1] = verts_skel[ii][1];
    model_verts[i][2] = verts_skel[ii][2];

    //    printf("(x,y,z) = (%.2f, %.2f, %.2f)\n",model_verts[i][0], model_verts[i][1],model_verts[i][2]);
   
    model_norms[i][0] = verts_skel[ii][0];
    model_norms[i][1] = verts_skel[ii][1];
    model_norms[i][2] = verts_skel[ii][2];

    model_uv[i][0] = st_skel[ii][0];
    model_uv[i][1] = st_skel[ii][1];

  }

/*
  printf("======================================================================\n");
  for (int i=0; i<10*3; i++) {
    int ii = ref[i];
    if (i % 3 == 0) {
      printf("\n");
    }
    printf("v(x,y,z) = (%.2f, %.2f, %.2f)\n",model_verts[i][0],
	   model_verts[i][1],model_verts[i][2]);
    printf("n(x,y,z) = (%.2f, %.2f, %.2f)\n",model_norms[i][0],
	   model_norms[i][1],model_norms[i][2]);
  }
*/

  return polygon_cnt;
}

// initialization
void init(void)
{
  MAR_GLSL_DEBUG = false;

  // GLSLプログラムを作成する
  marCreateGLSLProgram(&shader[GLSL_SOLID_COLOR].program, "vs4_center_disk.glsl", "fs4_eyelid.glsl");
  marCreateGLSLProgram(&shader[GLSL_SOLID_COLOR2].program, "vs4_twin_disk.glsl", "fs4_eyelid.glsl");
  marCreateGLSLProgram(&shader[GLSL_SHADING_TEX].program, "vs4_casual_shading_tex.glsl", "fs4_casual_shading_tex.glsl");

  // 0.8 をかけるのは目玉がまぶたを突き抜けないようにするため、眼球の大きさをコントロール
//  model_polygon_cnt = uvsphere_sc(32, 1.0);
//  model_polygon_cnt = uvsphere_sc(4, 1.0 * 0.8);
  model_polygon_cnt = uvsphere_sc(32, 1.0 * 0.8);

  // 背景色を指定する
  //  glClearColor(0.3, 0.4, 0.7, 1.0);
  glClearColor(0.2, 0.3, 0.6, 1.0);

  // 裏面を表示しない
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  const std::string IMAGE_DIR = "../images/";

  // 眼球
  const std::string eyeball_path = IMAGE_DIR + EYEBALL_PNG;
  m_texture[TEX_SRC] = loadTextureEx(eyeball_path.c_str(), GL_LINEAR);
  if (m_texture[TEX_SRC].id == 0) {
    fprintf(stderr, "ERROR: could not load the image: %s\n", eyeball_path.c_str());
    exit(EXIT_FAILURE);
  }
  img_width = m_texture[TEX_SRC].width;
  img_height = m_texture[TEX_SRC].height;

  // まぶたの UV チェッカー
  const std::string checker_path = IMAGE_DIR + CHECKER_PNG;
  m_texture[TEX_CHECKER] = loadTextureEx(checker_path.c_str(), GL_LINEAR);
  if (m_texture[TEX_CHECKER].id == 0) {
    fprintf(stderr, "ERROR: could not load the image: %s\n", checker_path.c_str());
    exit(EXIT_FAILURE);
  }

  //----------------------------------------------------------------------
  // Location
  //----------------------------------------------------------------------
  shader[GLSL_SOLID_COLOR].Enable();
  {
    shader[GLSL_SOLID_COLOR].SetLocation("aPosition");
    shader[GLSL_SOLID_COLOR].SetLocation("aNormal");
    shader[GLSL_SOLID_COLOR].SetLocation("aColor");
    shader[GLSL_SOLID_COLOR].SetLocation("aUV");
//    shader[GLSL_SOLID_COLOR].SetLocation("uMVPMatrix", 0);

    // ここで、マップの一覧表示とエラーの有無をチェック
    if (shader[GLSL_SOLID_COLOR].PrintLocations() != 0) {
      printf("Shader Error: ロケーションの取得に失敗しました。\n");
      exit(EXIT_FAILURE);
    }
  }
  shader[GLSL_SOLID_COLOR].Disable();

  shader[GLSL_SOLID_COLOR2].Enable();
  {
    shader[GLSL_SOLID_COLOR2].SetLocation("aPosition");
    shader[GLSL_SOLID_COLOR2].SetLocation("aNormal");
    shader[GLSL_SOLID_COLOR2].SetLocation("aColor");
    shader[GLSL_SOLID_COLOR2].SetLocation("aUV");
//    shader[GLSL_SOLID_COLOR2].SetLocation("uMVPMatrix", 0);

    // ここで、マップの一覧表示とエラーの有無をチェック
    if (shader[GLSL_SOLID_COLOR2].PrintLocations() != 0) {
      printf("Shader Error: ロケーションの取得に失敗しました。\n");
      exit(EXIT_FAILURE);
    }
  }
  shader[GLSL_SOLID_COLOR2].Disable();

  shader[GLSL_SHADING_TEX].Enable();
  {
    shader[GLSL_SHADING_TEX].SetLocation("aPosition");
    shader[GLSL_SHADING_TEX].SetLocation("aNormal");
    shader[GLSL_SHADING_TEX].SetLocation("aColor");
    shader[GLSL_SHADING_TEX].SetLocation("aUV");

    shader[GLSL_SHADING_TEX].SetLocation("uMVPMatrix");
    shader[GLSL_SHADING_TEX].SetLocation("uModelViewMatrix");
    shader[GLSL_SHADING_TEX].SetLocation("uViewMatrix");
    shader[GLSL_SHADING_TEX].SetLocation("uNormalMatrix");

    if (shader[GLSL_SHADING_TEX].PrintLocations() != 0) {
      printf("GLSL_SHADING_TEX: Error: ロケーションの取得に失敗しました。\n");
      exit(EXIT_FAILURE);
    }
  }
  shader[GLSL_SHADING_TEX].Disable();


/*
  // シェーダプログラムが作成されていることを確認する！！
  if (! glIsProgram(programID)) {
    printf("シェーダプログラムが不正です。（or 作成されていません。）\n");
  }

  // GLSL Attribute 変数
  int fsum = 0;
  a_pos = glGetAttribLocation(programID, "aPosition");
  fsum = fsum + ((unsigned int)a_pos) >> 31;
  a_normal = glGetAttribLocation(programID, "aNormal");
  fsum = fsum + ((unsigned int)a_normal) >> 31;
  a_color = glGetAttribLocation(programID, "aColor");
  fsum = fsum + ((unsigned int)a_color) >> 31;
  if (fsum > 0) {
    printf("attribute 変数の格納場所の取得に失敗\n");
    return;
  }
  // Attribute 変数を有効にする
  glEnableVertexAttribArray(a_pos);
  glEnableVertexAttribArray(a_normal);
  glEnableVertexAttribArray(a_color);
*/

  baum(12, 0, 0, 0);
  baum(12, 0, 1, 0);

  // カメラの近接限界: モデルの中心から最も遠い点までの距離(目の外径は約 1.0)
  // 設定しないと、ズームしたとき、カメラが目の中に入ってしまう
  camera.SetZoomLimitDistance(1.0);

  //----------------------------------------------------------------------
  // VBO 頂点 バッファ生成
  glGenBuffers(13, vbo_baum);
  glGenBuffers(13, vbo_baum_normal);

  // VBO 色 バッファ生成
  glGenBuffers(1, &vbo_colors);

  glGenBuffers(1, &ibo_baum);

  for (int i=0; i<2; i++) {
    // VBO 頂点
    glBindBuffer(GL_ARRAY_BUFFER, vbo_baum[i]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mBaum[i].vertex), mBaum[i].vertex, GL_STATIC_DRAW);

    // VBO 法線ベクトル
    glBindBuffer(GL_ARRAY_BUFFER, vbo_baum_normal[i]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mBaum[i].normal), mBaum[i].normal, GL_STATIC_DRAW);
  }
  // VBO 色
  glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
  glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

  // VBO UV (まぶたの全ブロックで共通)
  glGenBuffers(1, &vbo_baum_uv);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_baum_uv);
  glBufferData(GL_ARRAY_BUFFER, sizeof(baum_uv), baum_uv, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_baum);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mBaum[0].index), mBaum[0].index, GL_STATIC_DRAW);

  //----------------------------------------------------------------------
  // VAO: まぶた(インスタンシング)
  //   コアプロファイルでは VAO が必須なので、頂点属性の設定は VAO に記録する
  //----------------------------------------------------------------------
  glGenVertexArrays(1, &vao[VAO_EYELID_CENTER]);
  glGenVertexArrays(1, &vao[VAO_EYELID_TWIN]);
  {
    struct { int vao_id; int shader_id; int baum_index; } eyelid[] = {
      { VAO_EYELID_CENTER, GLSL_SOLID_COLOR,  0 },
      { VAO_EYELID_TWIN,   GLSL_SOLID_COLOR2, 1 },
    };
    for (auto &e : eyelid) {
      glBindVertexArray(vao[e.vao_id]);

      // 頂点
      glBindBuffer(GL_ARRAY_BUFFER, vbo_baum[e.baum_index]);
      glVertexAttribPointer(shader[e.shader_id]["aPosition"], 3, GL_FLOAT, GL_FALSE, 0, 0);
      glEnableVertexAttribArray(shader[e.shader_id]["aPosition"]);

      // 法線ベクトル
      glBindBuffer(GL_ARRAY_BUFFER, vbo_baum_normal[e.baum_index]);
      glVertexAttribPointer(shader[e.shader_id]["aNormal"], 3, GL_FLOAT, GL_FALSE, 0, 0);
      glEnableVertexAttribArray(shader[e.shader_id]["aNormal"]);

      // 色
      glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
      glVertexAttribPointer(shader[e.shader_id]["aColor"], 3, GL_FLOAT, GL_FALSE, 0, 0);
      glEnableVertexAttribArray(shader[e.shader_id]["aColor"]);

      // UV
      glBindBuffer(GL_ARRAY_BUFFER, vbo_baum_uv);
      glVertexAttribPointer(shader[e.shader_id]["aUV"], 2, GL_FLOAT, GL_FALSE, 0, 0);
      glEnableVertexAttribArray(shader[e.shader_id]["aUV"]);

      // IBO
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_baum);
    }
    glBindVertexArray(0);
  }

  //----------------------------------------------------------------------
  // 単色ならmodel_color を配列で持つのはもったいない。
  // 動的にメモリを取得する。
  // printf("sizeof(model_color) = %d\n", (int)sizeof(model_color));
  // バッファサイズ=ポリゴン数 * 3（3頂点で１ポリゴン) * 3(各頂点x,y,zの３要素) * sizeof(float)
  int bufsize = model_polygon_cnt * 9 * sizeof(float);
  printf("bufsize=%d\n", bufsize);
  float *model_color;
  model_color = (float *)malloc(bufsize);

  //----------------------------------------------------------------------
  // VAO
  //----------------------------------------------------------------------
//  glGenVertexArrays(1, &vao[VAO_IMAGE_PROC]);
  glGenVertexArrays(1, &vao[VAO_DRAW_MESH_TEX]);

  // 頂点数
  int verts_count = model_polygon_cnt * 3;
  // model_color を単色で埋める
  int ii;
  for (int i=0; i<verts_count; i++) {
    ii = i*3;
    model_color[ii] = 1.0f;
    model_color[ii+1] = 1.0f;
    model_color[ii+2] = 1.0f;
  }


  GLuint vbo_model2, vbo_model_color2, vbo_model_normal2, vbo_model_uv2;

  glBindVertexArray(vao[VAO_DRAW_MESH_TEX]);
  {
    // 頂点
    vbo_model2 = makeVBO(bufsize, &model_verts[0][0]);
    glVertexAttribPointer(shader[GLSL_SHADING_TEX]["aPosition"], 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_SHADING_TEX]["aPosition"]);

    // 色
    vbo_model_color2 = makeVBO(bufsize, model_color);
    glVertexAttribPointer(shader[GLSL_SHADING_TEX]["aColor"], 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_SHADING_TEX]["aColor"]);

    // 法線
    vbo_model_normal2 = makeVBO(bufsize, &model_norms[0][0]);
    glVertexAttribPointer(shader[GLSL_SHADING_TEX]["aNormal"], 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_SHADING_TEX]["aNormal"]);

    // UV
    vbo_model_uv2 = makeVBO(bufsize, &model_uv[0][0]);
    glVertexAttribPointer(shader[GLSL_SHADING_TEX]["aUV"], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(shader[GLSL_SHADING_TEX]["aUV"]);
  }
  glBindVertexArray(0);

  free(model_color);
}

/*
  program: シェーダプログラム
  name: uniform で glsl プログラムに渡す名前
  unit_no: テクスチャーユニットの番号
  texture: m_texture[]
*/
void bind_sampler(GLuint program, const char *name, GLint unit_no, GLuint texture)
{
  GLuint location = glGetUniformLocation(program, name);
//  assert(glGetError() == GL_NO_ERROR);
  glUniform1i(location, unit_no);
//  assert(glGetError() == GL_NO_ERROR);
  glActiveTexture(GL_TEXTURE0 + unit_no); // テクスチャユニット unit_no
  glBindTexture(GL_TEXTURE_2D, texture);
}

// smat4 の左上 3x3 を取り出す(法線行列用)
smat3 upperLeft3x3(const smat4 &mat)
{
  smat3 r;
  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      r.m[i][j] = mat.m[i][j];
    }
  }
  return r;
}

void display(void)
{
  // glClear() で指定した背景色で塗りつぶす
  //  glClear(GL_COLOR_BUFFER_BIT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	

  matget_perspective(camera.fov, camera.aspectRatio, camera.near_z, camera.far_z, m_proj);
  matget_lookat(camera.position, camera.target, camera.up_vector, m_view);


  int shaderTYPE = GLSL_SOLID_COLOR;
  gIndex = 0;

  // シェーダプログラムの指定
  shader[shaderTYPE].Enable();
  {
    glBindVertexArray(vao[VAO_EYELID_CENTER]);

    bind_sampler(shader[shaderTYPE].program, "image", 0, m_texture[TEX_CHECKER].id);
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uUseTexture"), gUseTexture ? 1.0f : 0.0f);

    //----------------------------------------------------------------------
    float tt;
    tt = sin(theta_sum); // tt range = [-1.0, 1.0], 周期の速さを *0.1 で調整している
    tt = (tt+1) / 2.0; // 0..1
    //  printf("%.2f\n", tt);

    //----------------------------------------------------------------------
    float degree;
    int divcnt = 12;
    float degree_step = 360/divcnt;

    glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uViewMatrix"), 1, GL_FALSE, &m_view.m[0][0]);
    //  glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uModelMatrix"), 1, GL_FALSE, &m_model[0]);
    glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uProjMatrix"), 1, GL_FALSE, &m_proj.m[0][0]);
    // m_view に m_mrotが反映されている
    // m_mrot が必要になるのはカメラではなく、モデルを回転するとき
    // m_mmrot としたほうが良い？ Matrix_MouseModelROT
    //  rotCamera.get_rotmatrix(m_mrot); // 不要
    //  glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uMMRot"), 1, GL_FALSE, &m_mrot[0]);
    glUniform3f(glGetUniformLocation(shader[shaderTYPE].program, "uModelCenter"), mBaum[gIndex].model_center.x,
                mBaum[gIndex].model_center.y,
                mBaum[gIndex].model_center.z);
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uTime"), tt);
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uRingBlocks"), 12);
    float InstanceCount = 12;
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uInstanceCount"), InstanceCount);

    // 6面 = 12 ポリゴン, 12ポリゴン　x ３頂点 = 36
    glDrawElementsInstanced(GL_TRIANGLES, 12*3, GL_UNSIGNED_INT, 0, InstanceCount);
    glBindVertexArray(0);
  }
  shader[shaderTYPE].Disable();

  shaderTYPE = GLSL_SOLID_COLOR2;
  gIndex = 1;

  // シェーダプログラムの指定
  shader[shaderTYPE].Enable();
  {
    glBindVertexArray(vao[VAO_EYELID_TWIN]);

    bind_sampler(shader[shaderTYPE].program, "image", 0, m_texture[TEX_CHECKER].id);
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uUseTexture"), gUseTexture ? 1.0f : 0.0f);

    //----------------------------------------------------------------------
    float tt;
    tt = sin(theta_sum); // tt range = [-1.0, 1.0], 周期の速さを *0.1 で調整している
    tt = (tt+1) / 2.0; // 0..1
    //  printf("%.2f\n", tt);

    //----------------------------------------------------------------------
    float degree;
    int divcnt = 12;
    float degree_step = 360/divcnt;

    glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uViewMatrix"), 1, GL_FALSE, &m_view.m[0][0]);
    //  glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uModelMatrix"), 1, GL_FALSE, &m_model[0]);
    glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uProjMatrix"), 1, GL_FALSE, &m_proj.m[0][0]);
    // m_view に m_mrotが反映されている
    // m_mrot が必要になるのはカメラではなく、モデルを回転するとき
    // m_mmrot としたほうが良い？ Matrix_MouseModelROT
    //  rotCamera.get_rotmatrix(m_mrot); // 不要
    //  glUniformMatrix4fv(glGetUniformLocation(shader[shaderTYPE].program, "uMMRot"), 1, GL_FALSE, &m_mrot[0]);
    glUniform3f(glGetUniformLocation(shader[shaderTYPE].program, "uModelCenter"), mBaum[gIndex].model_center.x,
                mBaum[gIndex].model_center.y,
                mBaum[gIndex].model_center.z);
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uTime"), tt);
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uRingBlocks"), 12);
    float InstanceCount = 12*2;
    glUniform1f(glGetUniformLocation(shader[shaderTYPE].program, "uInstanceCount"), InstanceCount);

    // 6面 = 12 ポリゴン, 12ポリゴン　x ３頂点 = 36
    glDrawElementsInstanced(GL_TRIANGLES, 12*3, GL_UNSIGNED_INT, 0, InstanceCount);
    glBindVertexArray(0);
  }
  shader[shaderTYPE].Disable();


  glBindVertexArray(vao[VAO_DRAW_MESH_TEX]);
  {
    shader[GLSL_SHADING_TEX].Enable();
    {
      // 眼球は、極(瞳孔)がカメラのほうを向くように、x 軸まわりに 90 度回す
      smat4 m_model;
      matget_rotate(getrad(90), true, false, false, m_model);

      smat4 m_mvp = m_model * m_view * m_proj;
      smat4 m_modelview = m_model * m_view;
      smat3 m_modelview33 = upperLeft3x3(m_modelview); // 法線用の 3x3

      bind_sampler(shader[GLSL_SHADING_TEX].program, "image", 0, m_texture[TEX_SRC].id);

      // ワールド変換行列
      glUniformMatrix4fv(shader[GLSL_SHADING_TEX]["uMVPMatrix"], 1, GL_FALSE, &m_mvp.m[0][0]);
      glUniformMatrix4fv(shader[GLSL_SHADING_TEX]["uModelViewMatrix"], 1, GL_FALSE, &m_modelview.m[0][0]);
      glUniformMatrix4fv(shader[GLSL_SHADING_TEX]["uViewMatrix"], 1, GL_FALSE, &m_view.m[0][0]);
      glUniformMatrix3fv(shader[GLSL_SHADING_TEX]["uNormalMatrix"], 1, GL_FALSE, &m_modelview33.m[0][0]);

      // ポリゴン数 x ３頂点
      //        glDrawElements(GL_TRIANGLES, model_polygon_cnt*3, GL_UNSIGNED_INT, 0);
      glDrawArrays(GL_TRIANGLES, 0, model_polygon_cnt*3);
    }
    shader[GLSL_SHADING_TEX].Disable();
  }
  glBindVertexArray(0);
}

void animate(void)
{
  // アニメーション関係のパラメータをここで計算する
  // framecountのカウントアップや、移動や回転のパラメータの計算もここで行う

  double delta_time = HRTimer.getELT();
  //  printf("delta_time = %.2f\n", delta_time);

  const float SPEED_CTL = 0.5;

  // １秒で 2PI 動いてね
  double theta = 2.0*M_PI * delta_time;
  // theta のトータル
  theta_sum += theta * SPEED_CTL;
}

// callback
void char_callback(GLFWwindow* window, unsigned int codepoint)
{
  switch(codepoint)
    {
    case 'q':
    case 'Q':
      running = GL_FALSE;
      break;

    case 't':
    case 'T':
      // まぶたのブロック: テクスチャ表示 / 頂点カラー表示
      gUseTexture = ! gUseTexture;
      break;

    default:
      break;
    }
}

void window_size_callback(GLFWwindow* window, int w, int h)
{
  glViewport(0, 0, w, h); // GL の描画領域を指定
  client_width = w;
  client_height = h;

  // ウィンドウのアスペクト比にモデルが影響されないようにする
  camera.setAspectRatio(client_width, client_height);
}

// Mouse Move
void cursor_position_callback(GLFWwindow *window, double xPos, double yPos)
{
  int x = (int)xPos;
  int y = (int)yPos;

  if (FMouseDown) {
    int dx = x - FSX;
    int dy = y - FSY;

    // モデルがプラスに動くということはカメラはマイナスに動くということ
    vvec3 axis;
    axis.x = -(2.0 * M_PI * (float)dy / client_height);
    axis.y = -(2.0 * M_PI * (float)dx / client_width);
    axis.z = 0.0;

    // 回転前に、回転軸に回転を加える
    axis = camera.getDragstartQuat() * axis;

    // 軸の長さを角度として扱う
    float theta = axis.length();
    if (theta < 1e-9f) {
      return; // 動いていない
    }

    // マウスドラッグ時の回転（マウスダウン時の回転状態を起点とした回転）
    qquat q_delta = {theta, axis};
    camera.dragRotate(q_delta);

  } else if (FMouseZoomDown) {
    // 3D-Coat スタイルのズーム: 右へドラッグでズームイン、左へドラッグでズームアウト
    float zoom_delta = x - FSX;

    // 目いっぱい(ウィンドウの幅だけ)ドラッグしたときの操作量を 10 とする
    float operation_amount = 10.0f * zoom_delta / client_width;

    // 操作量 10 で、距離が 0.1 倍 (ズームイン) / 10 倍 (ズームアウト) になる
    float base = distance100;
    float new_distance;
    if (zoom_delta > 0.0f) {
      new_distance = base * pow(0.794328, operation_amount);
    } else {
      new_distance = base * pow(1.258925, fabs(operation_amount));
    }

    camera.setDistance(new_distance - base);
  }
}

// Mouse Down/Up
//   左ドラッグ: カメラ回転 / 右ドラッグ: ズーム (3D-Coat のキーバインド)
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
  if (action != GLFW_PRESS) {
    // ボタンを離したら、そのドラッグを終える
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      FMouseDown = false;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      FMouseZoomDown = false;
    }
    return;
  }

  if (FMouseDown || FMouseZoomDown) {
    return;
  }

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    FMouseDown = true;
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    FMouseZoomDown = true;
  } else {
    return;
  }

  double xPos, yPos;
  glfwGetCursorPos(window, &xPos, &yPos);
  FSX = (int)xPos;
  FSY = (int)yPos;

  // ドラッグ開始時のカメラの状態(回転・距離)を記憶する
  camera.OnMouseDown(FSX, FSY);
  distance100 = camera.distance;
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

  //  client_width = 800;
  //client_height = 533;
  client_width = 1200;
  client_height = 800;
  window = glfwCreateWindow(client_width, client_height, "Eye Blink", NULL, NULL);
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
    fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
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

  // OpenGL 拡張を有効にしてから初期化処理を呼ぶ
  init();

  // 起動直後は window_size_callback が呼ばれないので、ここで反映する
  glViewport(0, 0, client_width, client_height);
  camera.setAspectRatio(client_width, client_height);

  // callback
  glfwSetWindowSizeCallback(window, window_size_callback);
  glfwSetCharCallback(window, char_callback);
  glfwSetCursorPosCallback(window, cursor_position_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);

  running = GL_TRUE;
  while (running && (! glfwWindowShouldClose(window))) {
    glfwPollEvents();

    animate();
    display();

    //ESCキーで終了
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      running = GL_FALSE;
    }
    glfwSwapBuffers(window);
  }

  quit_opengl();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

// 終了処理
void quit_opengl()
{
  // VAO の削除
  glDeleteVertexArrays(VAO_MAX, vao);

  // テクスチャの削除
  glDeleteTextures(1, &m_texture[TEX_SRC].id);
  glDeleteTextures(1, &m_texture[TEX_CHECKER].id);
  glDeleteBuffers(1, &vbo_baum_uv);

  // GLSLプログラムの削除
  for (int i=0; i<GLSL_MAX; i++) {
    glDeleteProgram(shader[i].program);
  }
}
