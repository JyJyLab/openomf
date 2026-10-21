// Android: libepoxy 대신 OpenGL ES 3.0 헤더를 바로 쓴다 (데스크톱 OpenGL 3.3 core 와 거의 같은 기능)
#pragma once
#include <stdbool.h> // libepoxy 헤더가 함께 넣어 주던 것
#include <stddef.h>
#include <stdint.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

// 팔레트 인덱스 프레임버퍼: GLES 3.0 에는 그릴 수 있는 16비트 정규화(RGBA16) 형식이 없으므로
// half float (EXT_color_buffer_half_float, 대부분의 안드로이드 GPU 지원) 로 바꾼다. 값은 index/1023 이라 정밀도 충분.
#ifndef GL_RGBA16
#define GL_RGBA16 GL_RGBA16F
#endif

// GLES 에는 glMultiDrawArrays 가 없다
static inline void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount) {
    for(GLsizei i = 0; i < drawcount; i++) {
        glDrawArrays(mode, first[i], count[i]);
    }
}
