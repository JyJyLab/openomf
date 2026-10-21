#ifndef BINDINGS_H
#define BINDINGS_H

#include "epoxy/gl.h"

void binding_active_tex(GLuint id);

void bindings_bind_vao(GLuint id);
void bindings_bind_vbo(GLuint id);
void bindings_bind_tex(GLuint unit, GLuint id);
void bindings_bind_fbo(GLuint id);

void bindings_unbind_vao(GLuint id);
void bindings_unbind_vbo(GLuint id);
void bindings_unbind_tex(GLuint unit, GLuint id);
void bindings_unbind_fbo(GLuint id);

// GL 컨텍스트가 새로 만들어졌을 때(안드로이드 백그라운드 복귀) 기억해 둔 바인딩을 잊는다
void bindings_reset(void);

#endif // BINDINGS_H
