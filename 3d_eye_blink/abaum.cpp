#include <stdio.h>
#include <math.h>
#include "abaum.h"

// iLevel = 0 -> index = 0
// iLevel = 1 -> index = 1..3
// iLevel = 2 -> index = 4..12
//( iLevel = 3 -> index = 13..)

int get_baum_index(int iLevel, int iy, int iz)
{
  int index = 0;
  int head = 0;
  if (iLevel == 0) {
    index = iy;
  } else {
    for (int i=0; i<iLevel; i++) {
      int seed = pow(3, i);
//      printf("seed = %d\n", seed);

      int katamari = seed * seed; // 1, 9, ...
//      printf("katamari = %d\n", katamari);
      float ruiseki_three_of_two = pow(2.0/3.0, i); // 2.0/3.0 の累積
//      printf("ruiseki_three_of_two = %.2f\n", ruiseki_three_of_two);
      head += katamari + (int)(katamari * ruiseki_three_of_two + 0.5); // head = 2, 17, 152
//      printf("head = %d\n", head);
    }
    index = head + iy*pow(3, iLevel) + iz;
  }

  return index;
/*
  int index = 0;
  int head = 0;
  for (int i=0; i<iLevel; i++) {
    head = head + pow(3, i);
  }
  index = head + z;
  return index;
*/
}


// 頂点の平均を求める。　//用途モデルの中心を求めるために使用
// 戻り値: すべての頂点の平均値
// 引数:
// verts: 頂点のポインタ
// cnt: 頂点数 6頂点なら、6。 xyz を含めた 18 としないこと。
vvec3 _calc_verts_mean(float *verts, int cnt)
{
  vvec3 *_verts = (vvec3 *)verts;
  //  cnt = cnt / 3;

  //  vec3_print2(*_verts);


  vvec3 sum(0.0f, 0.0f, 0.0f);
  for (int i=0; i<cnt; i++) {
    sum.x += _verts[i].x;
    sum.y += _verts[i].y;
    sum.z += _verts[i].z;
    //    printf("sum up: %.2f, %.2f,  %.2f, \n", sum.x, sum.y, sum.z);
  }

  sum.x /= cnt;
  sum.y /= cnt;
  sum.z /= cnt;

  //  vec3_print2(sum);

  return sum;
}

// verts: 頂点、vec_x, vert_count: 頂点数
vvec3 calc_model_center(float *verts, int vert_count)
{
  vvec3 model_center = _calc_verts_mean(verts, vert_count);
  return model_center;
}

TABaum::TABaum() {
}

void TABaum::setup_vertex(int N, int iLevel, int y, int z)
{
  double r = 1.0;
  double abaum_length = 0.38;

  const int Ny = pow(3, iLevel+1);
  int Ny_half = Ny/2.0;
  double delta_phi = M_PI/(2.0*Ny);

  const int NX = N * pow(3, iLevel);
  double delta_theta = -2.0*M_PI/NX;

  // 上面と下面の phi, 高さを求めるための種
  double phi_top = -delta_phi/2.0 + delta_phi*(y+1); // 上の円盤の phi
  double phi_bottom = -delta_phi/2.0 + delta_phi*y; // 下の円盤の phi

  // 上面と下面の半径
  double r_top = cos(phi_top)*r; // 上の円盤の半径
  double r_bottom = cos(phi_bottom)*r; // 下の円盤の半径

  double distance_top = r_top - abaum_length;
  double distance_bottom = r_bottom - abaum_length;

  double near_top = distance_top;
  double near_bottom = distance_bottom;
  double far_top = distance_top + abaum_length;
  double far_bottom = distance_bottom + abaum_length;

  if (iLevel > 0) {
    // バウムの奥行きを設定
    float len = abaum_length / pow(3, iLevel);
    
    near_top = distance_top + len * z;
    far_top = near_top + len;
    near_bottom = distance_bottom + len * z;
    far_bottom = near_bottom + len;
  }


  double theta, phi;
  int k;
 
  // 頂点の基本セットを用意する
  // baum を構成する頂点を用意する。重複なしの頂点データ。

  // x-z 平面におけるz軸は手前が＋なので、theta の回転方向を逆回りにする
  delta_theta = - 2.0*M_PI/(double)(NX);
  // outer
  for (int i=0; i<2; i++) {
    theta = delta_theta * i;

    // top
    k = i*2;
    verts_skel[k][0] = (float)(far_top * cos(theta));
    verts_skel[k][1] = sin(phi_top);
    verts_skel[k][2] = (float)(far_top * sin(theta));
    // printf("%.2f, %.2f, %.2f\n", verts_skel[k][0], verts_skel[k][1], verts_skel[k][2]);

    // bottom
    k = k+1;
    verts_skel[k][0] = (float)(far_bottom * cos(theta));
    verts_skel[k][1] = sin(phi_bottom);
    verts_skel[k][2] = (float)(far_bottom * sin(theta));
    // printf("%.2f, %.2f, %.2f\n", verts_skel[k][0], verts_skel[k][1], verts_skel[k][2]);
  }

  // inner
  for (int i=0; i<2; i++) {
    theta = delta_theta * i;

    // top
    k = i*2+4;
    verts_skel[k][0] = (float)(near_top * cos(theta));
    verts_skel[k][1] = sin(phi_top);
    verts_skel[k][2] = (float)(near_top * sin(theta));
    // printf("%.2f, %.2f, %.2f\n", verts_skel[k][0], verts_skel[k][1], verts_skel[k][2]);

    // bottom
    k = k+1;
    verts_skel[k][0] = (float)(near_bottom * cos(theta));
    verts_skel[k][1] = sin(phi_bottom);
    verts_skel[k][2] = (float)(near_bottom * sin(theta));
    // printf("%.2f, %.2f, %.2f\n", verts_skel[k][0], verts_skel[k][1], verts_skel[k][2]);
  }

  //----------------------------------------------------------------------
  // モデルを作るための頂点データ
  // verts_skel の頂点データを元にモデルを作るための頂点データを作る。
  // 面ごとに頂点を用意する。
  // 頂点データから面頂点を作成する
  for (int i=0; i<24; i++) {
    k = baumbaum[i]; // index
    vertex[i][0] = verts_skel[k][0];
    vertex[i][1] = verts_skel[k][1];
    vertex[i][2] = verts_skel[k][2];
  }

  model_center = calc_model_center(&verts_skel[0][0], 8);
}

void TABaum::setup_normal(int N, int iLevel, int y, int z)
{
  //----------------------------------------------------------------------
  // 法線ベクトルの基本セット用意する
  // baum を構成する法線ベクトルを用意する。重複なしの法線ベクトルデータ。
  //----------------------------------------------------------------------
  float theta, delta_theta;
  int k;

  // NX: N eXtend
  // iLevel に応じた分割数
  int NX;
  NX = N * pow(3, iLevel);

  delta_theta = - 2.0*M_PI/(double)(NX);
//  delta_theta = - 2.0*M_PI/(double)N;
  // outer temae
  theta = 0;
  norms_skel[0][0] = cos(theta);
  norms_skel[0][1] = 0;
  norms_skel[0][2] = sin(theta);
  // outer oku
  theta = delta_theta;
  norms_skel[1][0] = cos(theta);
  norms_skel[1][1] = 0;
  norms_skel[1][2] = sin(theta);
  // inner temae
  theta = 0;
  norms_skel[2][0] = -cos(theta);
  norms_skel[2][1] = 0;
  norms_skel[2][2] = -sin(theta);
  // inner oku
  theta = delta_theta;
  norms_skel[3][0] = -cos(theta);
  norms_skel[3][1] = 0;
  norms_skel[3][2] = -sin(theta);

  // top
  norms_skel[4][0] = 0;
  norms_skel[4][1] = 1;
  norms_skel[4][2] = 0;

  // side(temae)
  theta = 0;
  norms_skel[5][0] = sin(theta);
  norms_skel[5][1] = 0;
  norms_skel[5][2] = -(-cos(theta)); // z手前が+なので = 1

  // bottom
  norms_skel[6][0] = 0;
  norms_skel[6][1] = -1;
  norms_skel[6][2] = 0;

  // side(oku)
  theta = delta_theta;
  norms_skel[7][0] = -sin(theta);
  norms_skel[7][1] = 0;
  norms_skel[7][2] = -cos(theta); // z手前が+なので = -1

  /*
  printf("法線ベクトル\n");
  for (int i=0; i<8; i++) {
    printf("%.2f, %.2f, %.2f, \n", norms_skel[i][0], norms_skel[i][1], norms_skel[i][2]);
  }
  */

  // 各面頂点ごとに法線ベクトルを用意
  for (int i=0; i<24; i++) {
    k = baum_normnorm[i];
    normal[i][0] = norms_skel[k][0];
    normal[i][1] = norms_skel[k][1];
    normal[i][2] = norms_skel[k][2];
  }
}

