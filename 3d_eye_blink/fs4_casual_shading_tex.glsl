#version 430

layout (location = 0) in vec3 vColor;
layout (location = 1) in vec2 vUV;

layout (location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D image;

void main()
{
  vec3 tex_diffuse = texture(image, vUV).rgb;
//  tex_diffuse *= 0.001;

  vec3 col = vColor + vec3(1.0, 1.0, 1.0);
//  vec3 col = vColor*0.001 + vec3(vUV.x, vUV.y, 0.0);

  FragColor = vec4(col * tex_diffuse, 1.0);
//  FragColor = vec4(col + tex_diffuse, 1.0);
}
