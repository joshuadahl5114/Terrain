#version 150

uniform mat4 camera;
uniform mat4 modelview_matrix;

in vec3 a_Vertex;
in vec3 a_Color;
out vec4 color;

void main(void)
{
    vec4 pos = modelview_matrix * vec4(a_Vertex, 1.0);
    gl_Position = camera * pos;
    color = vec4(a_Color, 1.0);
}

