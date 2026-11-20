#version 450
layout(location = 0) out vec4 FragColor;
layout(binding = 0) uniform sampler2D src;

const float PI = 3.14159265358979323846;
const int N = 8;

void main()
{
  int index = int(gl_FragCoord.x) % N;
  vec2 sum = vec2(0.0);

  for (int k=0; k<N; k++) {
    vec2 data = texelFetch(src, ivec2(k, 0), 0).rg;
    float theta = 2.0 * PI * float(k * index) / float(N);
    float c = cos(theta);
    float s = sin(theta);

    // (a + ib) * (cos(theta) + i sin(theta))
    sum += vec2(c * data.r - s * data.g,
                s * data.r + c * data.g);
  }
  FragColor = vec4(sum / float(N), 0.0, 1.0);
}
