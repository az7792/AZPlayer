#define MA_NO_DECODING         // 禁用解码
#define MA_NO_ENCODING         // 禁用编码
#define MA_NO_GENERATION       // 禁用内置波形生成器（如正弦波、噪音生成）
#define MA_NO_RESOURCE_MANAGER // 禁用资源管理器（主要管文件加载）
#define MA_NO_NODE_GRAPH       // 禁用音频节点图（滤镜、效果链）
#define MA_NO_ENGINE           // 禁用高级声音管理引擎

// #define MA_DEBUG_OUTPUT // 启用 printf() 输出调试日志

// #define MA_USE_STDINT // *该宏需要定义在编译参数中，而不是写在这儿

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
