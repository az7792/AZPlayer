#version 330 core
// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

in vec2 TexCoord;

/*
    0=RGB_PACKED
    1=RGBA_PACKED

    // 以下类型每个分量独占一个平面
    2=RGB_PLANAR
    3=RGBA_PLANAR
    4=Y
    5=YA
    6=YUV
    7=YUVA
*/
uniform int pixFormat=-1; // 0=RGB 1=RGBA 2=YUVxxx 3=Y UV
uniform sampler2D yTex;// Y | R | RGB | RGBA
uniform sampler2D uTex;// U | G
uniform sampler2D vTex;// V | B
uniform sampler2D aTex;// A
uniform sampler2D subTex;// 字幕RGBA
uniform bool haveSubTex = false; // 是否有字幕纹理
uniform bool showSub = true;    // 是否渲染字幕


out vec4 FragColor;

// YUV -> RGB BT.709
vec3 yuv2rgb(float y, float u, float v) {
    y = y * 1.1643835616;
    return vec3(
        y + 1.7927410714 * v - 0.9729450750,
        y - 0.2132486143 * u - 0.5329093286 * v + 0.3014826655,
        y + 2.1124017857 * u - 1.1334022179
    );
}

void main()
{
    if(pixFormat == 0) { // RGB_PACKED
        FragColor = vec4(texture(yTex, TexCoord).rgb, 1.0);
    }
    else if(pixFormat == 1) { // RGBA_PACKED
        FragColor = texture(yTex, TexCoord);
    }
    else if(pixFormat == 2 || pixFormat == 3) { // RGB_PLANAR / RGBA_PLANAR
        float r = texture(yTex, TexCoord).r;
        float g = texture(uTex, TexCoord).r;
        float b = texture(vTex, TexCoord).r;
        float a = (pixFormat == 3) ? texture(aTex, TexCoord).r : 1.0;
        FragColor = vec4(r, g, b, a);
    }
    else if(pixFormat == 4 || pixFormat == 5) { // Y / YA
        float y = texture(yTex, TexCoord).r;
        float a = (pixFormat == 5) ? texture(uTex, TexCoord).r : 1.0;
        FragColor = vec4(y, y, y, a);
    }
    else if(pixFormat == 6 || pixFormat == 7) { // YUV / YUVA
        float y = texture(yTex, TexCoord).r;
        float u = texture(uTex, TexCoord).r;
        float v = texture(vTex, TexCoord).r;
        float a = (pixFormat == 7) ? texture(aTex, TexCoord).r : 1.0;
        FragColor = vec4(yuv2rgb(y, u, v), a);
    }
    else {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // 未知格式
    }

    if(haveSubTex && showSub) {
        vec4 subColor = texture(subTex, TexCoord);
        FragColor = mix(FragColor, subColor, subColor.a);
    }
}
