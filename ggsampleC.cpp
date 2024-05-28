//
// ゲームグラフィックス特論宿題アプリケーション
//
#include "GgApp.h"

// 度からラジアンに変換
inline float toRad(float degree)
{
  return degree * 3.14159265f / 180.0f;
}

// 光源データ
static GgSimpleShader::Light light
{
  { 0.2f, 0.2f, 0.2f, 1.0f }, // 環境光成分
  { 1.0f, 1.0f, 1.0f, 0.0f }, // 拡散反射光成分
  { 1.0f, 1.0f, 1.0f, 0.0f }, // 鏡面反射光成分
  { 20.0f, 200.0f, 50.0f, 1.0f }  // 光源位置
};

//
// Catmull-Rom Spline
//
//   x0, x1, x2, x3: 補間する値
//   t: パラメータ (0≦t≦1)
//   戻り値: x0, x1, x2, x3 を t で補間した値
//
static float catmull_rom(float x0, float x1, float x2, float x3, float t)
{
  // TODO: 設問（１）
  // 
  // この部分に x1 における速度を (x2 - x0) / 2, x2 における速度を (x3 - x1) / 2 として,
  // t = 0 のとき x1、t = 1 のとき x2 となる Catmull-Rom 曲線の, パラメータ t における値を
  // 求める処理を実装して, 次の (x2 - x1) * t + x1 の代わりに返すようにしてください.
  //
  return (x2 - x1) * t + x1;
}

//
// Catmull-Rom Spline による点列の補間
//
//   p0, p1, p2, p3: 補間する位置 (x, y, z) と方向 (d) の４つの要素を持つ配列
//   t: パラメータ (0≦t≦1)
//   p: p0, p1, p2, p3 を t で補間した値
//
static void interpolate(float* p, const float* p0, const float* p1, const float* p2, const float* p3, float t)
{
  p[0] = catmull_rom(p0[0], p1[0], p2[0], p3[0], t);
  p[1] = catmull_rom(p0[1], p1[1], p2[1], p3[1], t);
  p[2] = catmull_rom(p0[2], p1[2], p2[2], p3[2], t);
  p[3] = catmull_rom(p0[3], p1[3], p2[3], p3[3], t);
}

//
// 任意の数の点列の Catmull-Rom Spline による補間
// 
//   p: 補間する点列の座標値
//   t: 補間する点列の各点の時刻 (タイムライン, 値は昇順に格納されている)
//   n: 点の数
//   u: 補間値を得るパラメータ (t[0]≦u≦t[n - 1]）
//   q: 補間した値
//
void curve(float q[4], const float p[][4], const float t[], int n, float u)
{
  // 点の数が 0 だったら何もしない
  if (--n < 0) return;

  // 点の数が 1 以上の時
  if (n == 0)
  {
    // 点の数が１個だったらその点の値を返す
    q[0] = p[0][0];
    q[1] = p[0][1];
    q[2] = p[0][2];
    q[3] = p[0][3];
  }
  else {
    // 点が２個以上あるなら
    int i = 0, j = n;

    // u を含む t の区間 [t[i], t[i+1]) を二分法で求める
    while (i < j)
    {
      // 中間の点の番号
      const int k{ (i + j) / 2 };

      // u が中間の点の時刻より後だったら
      if (t[k] < u)
      {
        // 最初の点を中間の点の後にする
        i = k + 1;
      }
      else
      {
        // 最後の点を中間の点にする
        j = k;
      }
    }

    // u は i 番目の点の時刻よりも前なので i から 1 を引くが負になったら 0 にしておく
    if (--i < 0) i = 0;

    // 時刻が最後の点の時刻より前なら
    if (i < n)
    {
      // ひとつ前の点
      int i0{ i - 1 };
      if (i0 < 0) i0 = 0;

      // 開始点
      int i1{ i };

      // 終了点
      int i2{ i + 1 };

      // ひとつ次の点
      int i3{ i + 2 };
      if (i3 > n) i3 = n;

      // 区間内のパラメータ
      const float s{ (u - t[i1]) / (t[i2] - t[i1]) };

#if 0
      // タイムラインもスプライン補間する場合
      interpolate(q, p[i0], p[i1], p[i2], p[i3],
        catmull_rom(t[i0], t[i1], t[i2], t[i3], s) - t[i1]);
#else
      // タイムラインは線形（折れ線）補間する場合
      interpolate(q, p[i0], p[i1], p[i2], p[i3], s);
#endif
    }
    else
    {
      // 時刻が最後の点の時刻より後なので最後の点の位置を使う
      q[0] = p[n][0];
      q[1] = p[n][1];
      q[2] = p[n][2];
      q[3] = p[n][3];
    }
  }
}

//
// アプリケーション本体
//
int GgApp::main(int argc, const char* const* argv)
{
  // ウィンドウを作成する (この行は変更しないでください)
  Window window{ argc > 1 ? argv[1] : "ggsampleC", 1280, 720 };

  // 図形データを読み込む (大きさを正規化しない)
  GgSimpleObj course{ "course.obj", false };
  GgSimpleObj vehicle{ "vehicle.obj", false };

  // シェーダを作成する
  const GgSimpleShader simple{ "simple.vert", "simple.frag" };

  // 光源データから光源のバッファオブジェクトを作成する
  GgSimpleShader::LightBuffer lightBuffer{ light };

  // ビュー変換行列を設定する
  const GgMatrix mv{ ggLookat(-80.0f, 120.0f, 30.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f) };

  // 背景色を設定する
  glClearColor(0.1f, 0.2f, 0.3f, 0.0f);

  // 隠面消去処理を設定する
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  // 現在時刻を初期化する
  glfwSetTime(0.0);

  // ウィンドウが開いている間繰り返す
  while (window)
  {
    // ウィンドウを消去する
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 投影変換行列を設定する
    const GgMatrix mp{ ggPerspective(0.5f, window.getAspect(), 10.0f, 300.0f) };

    // シェーダプログラムを指定する
    simple.use(mp, mv, lightBuffer);

    // コースを描画する
    course.draw();

    // アニメーションの周期 (タイムラインの最後の時刻)
    constexpr auto cycle{ 5.0 };

    // 現在時刻
    const auto t{ static_cast<float>(fmod(glfwGetTime(), cycle)) };

    // TODO: 設問（２）
    //
    // 以下の車両の姿勢 (位置と方向) p を現在時刻 t で変化させて、
    // アニメーションの周期 cycle ごとに車がコース上を一周するようにしなさい。
    //
    float p[4]
    {
      -13.0f, 0.0f, -22.0f, // 位置
      toRad(-150.0f)        // 方向
    };

    // 車両の姿勢
    const GgMatrix mm{ ggTranslate(p[0], p[1], p[2]).rotateY(p[3]) };

    // 車両のモデルビュー変換行列を設定する
    simple.loadModelviewMatrix(mv * mm);

    // 車両を描画する
    vehicle.draw();

    // カラーバッファを入れ替えてイベントを取り出す
    window.swapBuffers();
  }

  return 0;
}
