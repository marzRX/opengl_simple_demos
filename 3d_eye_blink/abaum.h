#include "marMatrix2.h"

int get_baum_index(int iLevel, int y, int z);

class TABaum {
private:
  float verts_skel[8][3];
  float norms_skel[8][3];

// 立方体の頂点は、8頂点であるが、
// 各面がそれぞれ独立した4頂点をもつと考える。そうしないと、ソリッドカラーや、法線ベクトルを与えられない。
// 6面x4頂点=24頂点。
//
// 立方体において１つの頂点は３つの面に属する。
// これらの頂点を１つのデータとしてしまうと、面ごとにソリッドなカラーを与えることができない。
// また、隣り合う面で法線ベクトルを共有できるのは、面が滑らかに繋がっているとき（補間させるとき）で、
// 面がカッチリ別れているときは法線ベクトルを共有できない。
int baumbaum[24] = {
  0,1,2,3,
  4,5,6,7,
  6,4,2,0, // 8,9,10,11
  4,5,0,1, // 12,13,14,15
  5,7,1,3, // 16,17,18,19
  7,6,3,2, // 20,21,22,23
};

// 各面の頂点が参照する法線ベクトルのリスト
int baum_normnorm[24] = {
  0,0,1,1,
  2,2,3,3,
  4,4,4,4,
  5,5,5,5,
  6,6,6,6,
  7,7,7,7,
};

public:
  TABaum();

  vvec3 model_center;

  // 6面x4頂点,xyz
  float vertex[24][3]; // 重複なしの頂点
  float normal[24][3]; // 重複なしの法線ベクトル

  // iLevel (= Iterate Level)
  void setup_vertex(int N, int iLevel, int y, int z);
  void setup_normal(int N, int iLevel, int y, int z);

  // 6面x2ポリゴン(1面は、2ポリゴン) 
  // この index 値はスタティックでもつべき？
  // すべてのオブジェクトで同じことをしている
  unsigned int index[12][3] = {{0,1,2},{2,1,3}, // outer
			       {6,7,4},{4,7,5}, // inner
			       {8,9,10},{10,9,11}, // top
			       {12,13,14},{14,13,15}, // side(temae)
			       {16,17,18},{18,17,19}, // bottom
			       {20,21,22},{22,21,23}}; // side(oku)
};

/*
  //----------------------------------------------------------------------
  // 
  int magic;
  // index の作成
  // outer
  index_baum[0][0] = 0;
  index_baum[0][1] = 1;
  index_baum[0][2] = 2;

  index_baum[1][0] = 2;
  index_baum[1][1] = 1;
  index_baum[1][2] = 3;

  // inner
  index_baum[2][0] = 6;
  index_baum[2][1] = 7;
  index_baum[2][2] = 4;

  index_baum[3][0] = 4;
  index_baum[3][1] = 7;
  index_baum[3][2] = 5;

  // top
  index_baum[4][0] = 8;
  index_baum[4][1] = 9;
  index_baum[4][2] = 10;

  index_baum[5][0] = 10;
  index_baum[5][1] = 9;
  index_baum[5][2] = 11;

  // side(temae)
  index_baum[6][0] = 12;
  index_baum[6][1] = 13;
  index_baum[6][2] = 14;

  index_baum[7][0] = 14;
  index_baum[7][1] = 13;
  index_baum[7][2] = 15;

  // bottom
  index_baum[8][0] = 16;
  index_baum[8][1] = 17;
  index_baum[8][2] = 18;

  index_baum[9][0] = 18;
  index_baum[9][1] = 17;
  index_baum[9][2] = 19;

  // side(oku)
  index_baum[10][0] = 20;
  index_baum[10][1] = 21;
  index_baum[10][2] = 22;

  index_baum[11][0] = 22;
  index_baum[11][1] = 21;
  index_baum[11][2] = 23;
*/
