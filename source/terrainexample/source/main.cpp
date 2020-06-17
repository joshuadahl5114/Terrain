/*
 main
 
 Copyright 2012 Thomas Dalling - http://tomdalling.com/

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include "platform.hpp"

// third-party libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// standard C++ libraries
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <map>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// tdogl classes
#include "tdogl/Program.h"
#include "tdogl/Texture.h"
#include "tdogl/Camera.h"

using std::vector;
using std::string;
using std::ifstream;
using std::map;

// constants
const glm::vec2 SCREEN_SIZE(1024, 768);

// globals
GLFWwindow* gWindow = NULL;
tdogl::Camera gCamera;
double gScrollY = 0.0;
tdogl::Texture* gTexture = NULL;
tdogl::Program* gProgram = NULL;

GLuint gVAO = 0;

GLuint m_vertexBuffer;
GLuint m_indexBuffer;
GLuint m_colorBuffer;

GLfloat gDegreesRotated = 0.0f;

struct Vertex
{
    float x, y, z;
    Vertex(float x, float y, float z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }
};


struct Color
{
    float r, g, b;
    Color(float r, float g, float b)
    {
        this->r = r;
        this->g = g;
        this->b = b;
    }
};

//Let's try something else, instead of using the structs which probably aren't mapping correctly
//instead lets create a Vector of GLFloats for the color
vector<GLfloat> m_verts;
vector<GLfloat> m_colrs;

vector<Vertex> m_vertices;
vector<unsigned int> m_indices;
vector<Color> m_colors;

static void Update(float secondsElapsed) {
    //rotate the cube
    const GLfloat degreesPerSecond = 180.0f;
    gDegreesRotated += secondsElapsed * degreesPerSecond;
    while(gDegreesRotated > 360.0f) gDegreesRotated -= 360.0f;
    
    //move position of camera based on WASD keys, and XZ keys for up and down
    const float moveSpeed = 2.0; //units per second
    if(glfwGetKey(gWindow, 'S')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * -gCamera.forward());
    } else if(glfwGetKey(gWindow, 'W')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * gCamera.forward());
    }
    if(glfwGetKey(gWindow, 'A')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * -gCamera.right());
    } else if(glfwGetKey(gWindow, 'D')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * gCamera.right());
    }
    if(glfwGetKey(gWindow, 'Z')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * -glm::vec3(0,1,0));
    } else if(glfwGetKey(gWindow, 'X')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * glm::vec3(0,1,0));
    }
    
    //rotate camera based on mouse movement
    const float mouseSensitivity = 0.1f;
    double mouseX, mouseY;
    glfwGetCursorPos(gWindow, &mouseX, &mouseY);
    gCamera.offsetOrientation(mouseSensitivity * (float)mouseY, mouseSensitivity * (float)mouseX);
    glfwSetCursorPos(gWindow, 0, 0); //reset the mouse, so it doesn't go out of the window
    
    //increase or decrease field of view based on mouse wheel
    const float zoomSensitivity = -0.2f;
    float fieldOfView = gCamera.fieldOfView() + zoomSensitivity * (float)gScrollY;
    if(fieldOfView < 5.0f) fieldOfView = 5.0f;
    if(fieldOfView > 130.0f) fieldOfView = 130.0f;
    gCamera.setFieldOfView(fieldOfView);
    gScrollY = 0;
}

// loads the vertex shader and fragment shader, and links them to make the global gProgram
static void LoadShaders() {
    std::vector<tdogl::Shader> shaders;
    shaders.push_back(tdogl::Shader::shaderFromFile(ResourcePath("basic-fixed.vert"), GL_VERTEX_SHADER));
    shaders.push_back(tdogl::Shader::shaderFromFile(ResourcePath("basic-fixed.frag"), GL_FRAGMENT_SHADER));
    gProgram = new tdogl::Program(shaders);
}

static vector<float> LoadHeightMap(const string& rawFile, int width)
{
    //glGenVertexArrays(1, &gVAO);
    //glBindVertexArray(gVAO);
    
    const float HEIGHT_SCALE = 10.0f;
    std::ifstream fileIn(rawFile.c_str(), std::ios::binary);
    
    if (!fileIn.good())
    {
        std::cout << "File does not exist" << std::endl;
        //return null;
        //throw here instead
    }
    
    //This line reads in the whole file into a string
    string stringBuffer(std::istreambuf_iterator<char>(fileIn), (std::istreambuf_iterator<char>()));
    
    fileIn.close();
    
    if (stringBuffer.size() != (width * width))
    {
        std::cout << "Image size does not match passed width" << std::endl;
       //throw here instead
    }
    
    vector<float> heights;
    heights.reserve(width * width); //Reserve some space (faster)
    
    //Go through the string converting each character to a float and scale it
    for (int i = 0; i < (width * width); ++i)
    {
        //Convert to floating value, the unsigned char cast is importantant otherwise the values wrap at 128
        float value = (float)(unsigned char)stringBuffer[i] / 256.0f;
        
        heights.push_back(value * HEIGHT_SCALE);
        m_colors.push_back(Color(0.0f, value, 0.0f));
        //add each individually
        m_colrs.push_back(0.0f);
        m_colrs.push_back(value);
        m_colrs.push_back(0.0f);
    }
    // unbind the VAO
    //GenerateVertices(heights, width);
    //GenerateIndices(width);
    
    return heights;
}

static void GenerateVertices(const vector<float> heights, int width)
{
    //Generate the vertices
    int i = 0;
    float temp;
    float temp2;
    for (float z = 0; z <= (width -1)/10.0; z+= .1)
    {
        for (float x = 0; x <= (width-1)/10.0; x+= .1)
        {
            temp = heights[i++];
            temp2 = z - 99.0f;
            m_vertices.push_back(Vertex(x, temp, temp2));
            //add each individually
            m_verts.push_back(x);
            m_verts.push_back(temp);
            m_verts.push_back(temp2);
        }
    }
   
}

static void GenerateIndices(int width)
{
    /*
     We loop through building the triangles that
     make up each grid square in the heightmap
     
     (z*w+x) *----* (z*w+x+1)
     |   /|
     |  / |
     | /  |
     ((z+1)*w+x)*----* ((z+1)*w+x+1)
     */
    //Generate the triangle indices
    for (int z = 0; z < width - 1; ++z) //Go through the rows - 1
    {
        for (int x = 0; x < width - 1; ++x) //And the columns - 1
        {
            m_indices.push_back((z * width) + x); //Current point
            m_indices.push_back((z * width) + x + 1 + width);
            m_indices.push_back((z * width) + x + width); //adjacent point
            
            m_indices.push_back((z * width) + x); //Current point
            m_indices.push_back((z * width) + x + 1); //adjacent point
            m_indices.push_back((z * width) + x + 1 + width); //point above
           //point above
        }
    }
    
    //glGenBuffers(1, &m_indexBuffer);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * m_indices.size(), &m_indices[0], GL_STATIC_DRAW);
}

static void CreateVerticesAndIndices()
{
    m_verts.push_back(-.6f);
    m_verts.push_back(0.0f);
    m_verts.push_back(-90.0f);
    
    m_verts.push_back(.35f);
    m_verts.push_back(0.0f);
    m_verts.push_back(-90.0f);
    
    m_verts.push_back(0.0f);
    m_verts.push_back(.6f);
    m_verts.push_back(-90.0f);
    
    m_verts.push_back(.6f);
    m_verts.push_back(0.0f);
    m_verts.push_back(-90.0f);
    
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);
    
    m_indices.push_back(1);
    m_indices.push_back(3);
    m_indices.push_back(2);

}

// loads a cube into the VAO and VBO globals: gVAO and gVBO
static void LoadTerrain() {
    glGenVertexArrays(1, &gVAO);
    glBindVertexArray(gVAO);
    
    // make and bind the VAO, done in LoadHeightMap
    vector<float> heights = LoadHeightMap("heightmap.raw", 65);
    GenerateVertices(heights, 6);
    //CreateVerticesAndIndices();
    
    glGenBuffers(1, &m_vertexBuffer); //Generate a buffer for the vertices
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer); //Bind the vertex buffer
    GLfloat vertexData[] = {
        //  X     Y     Z
        0.0f, 0.8f, -50.0f,
        -0.8f,-0.8f, -50.0f,
        0.8f,-0.8f, -50.0f,
    };

    //glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * m_verts.size(), &m_verts[0], GL_STATIC_DRAW); //Send the data to OpenGL
    
    glEnableVertexAttribArray(gProgram->attrib("a_Vertex"));
    glVertexAttribPointer(gProgram->attrib("a_Vertex"), 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), NULL);
    //string attribName = "a_Vertex";
    //glBindAttribLocation(gProgram->object(), gProgram->attrib("a_Vertex"), attribName.c_str());
    
    glGenBuffers(1, &m_colorBuffer); //Generate a buffer for the colors
    glBindBuffer(GL_ARRAY_BUFFER, m_colorBuffer); //Bind the color buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * m_colrs.size(), &m_colrs[0], GL_STATIC_DRAW);
    //attribName = "a_Color";
    //glBindAttribLocation(gProgram->object(), gProgram->attrib("a_Color"), attribName.c_str());
    
    // connect the xyz to the "vert" attribute of the vertex shader
    glEnableVertexAttribArray(gProgram->attrib("a_Color"));
    glVertexAttribPointer(gProgram->attrib("a_Color"), 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), NULL);
    
    GenerateIndices(6);
    
    //we are going to create this things from scratch
    
    // make and bind the VBO
    //glGenBuffers(1, &gVBO);
    //glBindBuffer(GL_ARRAY_BUFFER, gVBO);

   
    //glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), &m_vertices[0], GL_STATIC_DRAW);

    // connect the xyz to the "vert" attribute of the vertex shader
  
    
    //glGenVertexArrays(1, &gVIO);
    //glBindVertexArray(gVIO);
    
    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * m_indices.size(), &m_indices[0], GL_STATIC_DRAW);
 
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void RenderHelper()
{
    // bind the VAO (the triangle)
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    //glVertexPointer(3, GL_FLOAT, 0, 0);
    //glEnableVertexAttribArray(gProgram->attrib("a_Vertex"));
    glVertexAttribPointer(gProgram->attrib("a_Vertex"), 3, GL_FLOAT, GL_FALSE, 0, 0);
    
    
    glBindBuffer(GL_ARRAY_BUFFER, m_colorBuffer);
    //glEnableVertexAttribArray(gProgram->attrib("a_Color"));
    glVertexAttribPointer(gProgram->attrib("a_Color"), 3, GL_FLOAT, GL_FALSE, 0, 0);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    
    //glBindVertexArray(gVAO);
    // draw the VAO
    //glDrawArrays(GL_TRIANGLES, 0, 6*2*3);
    glDrawElements(GL_TRIANGLES, (unsigned int)m_indices.size(), GL_UNSIGNED_INT, 0);
    
    // unbind the VAO, the program and the texture
    //glBindVertexArray(0);
}

static GLuint getUniformLocation(const string& name)
{
    GLuint location = glGetUniformLocation(gProgram->object(), name.c_str());
    
    return location;
}

static void sendUniform(const string& name, const float* matrix, bool transpose=false)
{
    GLuint location = getUniformLocation(name);
    glUniformMatrix4fv(location, 1, transpose, matrix);
}

static void UseOpenGLMatriceGeneration()
{
    float modelviewMatrix[16];
    float projectionMatrix[16];
    
    glLoadIdentity();
    
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glTranslatef(0.0f, -20.0f, 0.0f);
    glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
    //Translate using our zPosition variable
    glTranslatef(0.0, 0.0, 1.0f);
    
    //Get the current matrices from OpenGL
    glGetFloatv(GL_MODELVIEW_MATRIX, modelviewMatrix);
    glGetFloatv(GL_PROJECTION_MATRIX, projectionMatrix);
    
    sendUniform("modelview_matrix", modelviewMatrix);
    sendUniform("projection_matrix", projectionMatrix);
}

static void  UseGLMToGenerateMatrices()
{
    //for the terrain program
    glm::mat4 ident;
    
     // set the "camera" uniform
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 12.0f));
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(50.0f), 4.0f/3.0f, .01f, 100.0f);
    //gProgram->setUniform("projection_matrix", projectionMatrix);
    gProgram->setUniform("camera", gCamera.matrix());
    
    // set the "model" uniform in the vertex shader, based on the gDegreesRotated global
    gProgram->setUniform("modelview_matrix", glm::rotate(translate, glm::radians(0.75f), glm::vec3(0,1,0)));
}

// draws a single frame
static void Render() {
    // clear everything
    glClearColor(1, 1, 1, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    //uncomment for wireframe mode
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    
    // bind the program (the shaders)
    gProgram->use();
    
    
    UseGLMToGenerateMatrices();
    //UseOpenGLMatriceGeneration();
     glBindVertexArray(gVAO);
 
    
    RenderHelper();
    
    //glDrawElements(GL_TRIANGLES, (unsigned int)m_indices.size(), GL_UNSIGNED_INT, 0);
    //glDrawArrays(GL_TRIANGLES, 0, 6*2*3);
    
    glBindVertexArray(0);
    gProgram->stopUsing();
    
    // swap the display buffers (displays what was just drawn)
    glfwSwapBuffers(gWindow);
}



// records how far the y axis has been scrolled
void OnScroll(GLFWwindow* window, double deltaX, double deltaY) {
    //don't capture mouse events, find where it's wired up and get rid of it
    //gScrollY += deltaY;
}

void OnError(int errorCode, const char* msg) {
    throw std::runtime_error(msg);
}

// the program starts here
void AppMain() {
    // initialise GLFW
    glfwSetErrorCallback(OnError);
    if(!glfwInit())
        throw std::runtime_error("glfwInit failed");
    
    // open a window with GLFW
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    gWindow = glfwCreateWindow((int)SCREEN_SIZE.x, (int)SCREEN_SIZE.y, "OpenGL Tutorial", NULL, NULL);
    if(!gWindow)
        throw std::runtime_error("glfwCreateWindow failed. Can your hardware handle OpenGL 3.2?");

    // GLFW settings
    //glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    //glfwSetCursorPos(gWindow, 0, 0);
    //glfwSetScrollCallback(gWindow, OnScroll);
    glfwMakeContextCurrent(gWindow);

    // initialise GLEW
    glewExperimental = GL_TRUE; //stops glew crashing on OSX :-/
    if(glewInit() != GLEW_OK)
        throw std::runtime_error("glewInit failed");
    
    // GLEW throws some errors, so discard all the errors so far
    while(glGetError() != GL_NO_ERROR) {}

    // print out some info about the graphics drivers
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // make sure OpenGL version 3.2 API is available
    if(!GLEW_VERSION_3_2)
        throw std::runtime_error("OpenGL 3.2 API is not available.");

    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // load vertex and fragment shaders into opengl
    LoadShaders();

    // create buffer and fill it with the points of the triangle
    LoadTerrain();

    gCamera.setPosition(glm::vec3(0,0,4));
    gCamera.setViewportAspectRatio(SCREEN_SIZE.x / SCREEN_SIZE.y);
    
    
    double lastTime = glfwGetTime();
    while(!glfwWindowShouldClose(gWindow)){
        // process pending events
        glfwPollEvents();
        
        double thisTime = glfwGetTime();
        Update((float)(thisTime - lastTime));
        lastTime = thisTime;
        
        // draw one frame
        Render();

        // check for errors
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
            std::cerr << "OpenGL Error " << error << std::endl;

        //exit program if escape key is pressed
        if(glfwGetKey(gWindow, GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(gWindow, GL_TRUE);
    }

    // clean up and exit
    glfwTerminate();
}


int main(int argc, char *argv[]) {
    try {
        AppMain();
    } catch (const std::exception& e){
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
