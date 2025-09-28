#version 330 core
layout(location = 0) in vec2 aPos; // 顶点坐标
layout(location = 1) in vec2 aTexCoord; // 纹理坐标

uniform mat4 transform=mat4(1.0); //变换矩阵，控制旋转缩放移动

out vec2 TexCoord;// 输出到片段着色器
void main(){
    gl_Position = transform * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
