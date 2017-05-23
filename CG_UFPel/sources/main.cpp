// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <set>
#include <queue>
#include <algorithm>
#include <stack>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>
GLFWwindow* g_pWindow;
unsigned int g_nWidth = 1024, g_nHeight = 768;

// Include AntTweakBar
#include <AntTweakBar.h>
TwBar *g_pToolBar;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <shader.hpp>
#include <texture.hpp>
#include <controls.hpp>
#include <objloader.hpp>
#include <vboindexer.hpp>
#include <glerror.hpp>

class Vertex {

public:
	unsigned short vertex;
	std::set <unsigned short> neighbours;
	//Constructor
	Vertex(unsigned short vertice, std::set<unsigned short> neighbours) : vertex(vertice), neighbours(neighbours) { }
};

class Step {
public:
	std::vector <unsigned short> indices;
	std::vector <glm::vec3> indexed_vertices;
	std::vector <glm::vec2> indexed_uvs;
	std::vector <glm::vec3> indexed_normals;

	Step(std::vector <unsigned short> indices, std::vector <glm::vec3> indexed_vertices,
		std::vector <glm::vec2> indexed_uvs, std::vector <glm::vec3> indexed_normals) : indices(indices),
		indexed_vertices(indexed_vertices), indexed_uvs(indexed_uvs), indexed_normals(indexed_normals) {};
};


struct OrderNeighbours {
	bool operator()(Vertex const& n1, Vertex const& n2) {
		return n1.neighbours.size() > n2.neighbours.size() || n1.neighbours.size() == n2.neighbours.size() && n1.vertex > n2.vertex;
	}
};

//Reduce triangle 
bool reducePolygon(std::vector<unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices,
	std::vector <glm::vec2>& indexed_uvs, std::vector<glm::vec3>& indexed_normals,
	std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>& queue_vertex, std::set <unsigned short>& neighbours,
	std::stack <Step>& steps);

//Create Priority Queue based on the less neighbours' number considering indices
void createPriorityQueue(std::vector<unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices,
	std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>& queue_vertex, std::set <unsigned short>& neighbours);

Vertex takeVertexSubstitute(std::set <unsigned short> neighbours, std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>& queue_vertex);

void halfEdgeCollapse(std::vector <unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices, std::vector<glm::vec2> indexed_uvs,
	std::vector<glm::vec3>& normals, unsigned short remove_vertex, unsigned short vertex_substitute);

//Inverse of halfEdgeCollapse
bool vertexSplit(std::vector<unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices,
	std::vector <glm::vec2>& indexed_uvs, std::vector<glm::vec3>& indexed_normals, std::stack <Step>& steps);

void WindowSizeCallBack(GLFWwindow *pWindow, int nWidth, int nHeight) {

	g_nWidth = nWidth;
	g_nHeight = nHeight;
	glViewport(0, 0, g_nWidth, g_nHeight);
	TwWindowSize(g_nWidth, g_nHeight);
}

int main(void)
{
	int nUseMouse = 0;

	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	g_pWindow = glfwCreateWindow(g_nWidth, g_nHeight, "CG UFPel", NULL, NULL);
	if (g_pWindow == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(g_pWindow);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	check_gl_error();//OpenGL error from GLEW

					 // Initialize the GUI
	TwInit(TW_OPENGL_CORE, NULL);
	TwWindowSize(g_nWidth, g_nHeight);

	// Set GLFW event callbacks. I removed glfwSetWindowSizeCallback for conciseness
	glfwSetMouseButtonCallback(g_pWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW); // - Directly redirect GLFW mouse button events to AntTweakBar
	glfwSetCursorPosCallback(g_pWindow, (GLFWcursorposfun)TwEventMousePosGLFW);          // - Directly redirect GLFW mouse position events to AntTweakBar
	glfwSetScrollCallback(g_pWindow, (GLFWscrollfun)TwEventMouseWheelGLFW);    // - Directly redirect GLFW mouse wheel events to AntTweakBar
	glfwSetKeyCallback(g_pWindow, (GLFWkeyfun)TwEventKeyGLFW);                         // - Directly redirect GLFW key events to AntTweakBar
	glfwSetCharCallback(g_pWindow, (GLFWcharfun)TwEventCharGLFW);                      // - Directly redirect GLFW char events to AntTweakBar
	glfwSetWindowSizeCallback(g_pWindow, WindowSizeCallBack);

	//create the toolbar
	g_pToolBar = TwNewBar("CG UFPel ToolBar");
	// Add 'speed' to 'bar': it is a modifable (RW) variable of type TW_TYPE_DOUBLE. Its key shortcuts are [s] and [S].
	double speed = 0.0;
	TwAddVarRW(g_pToolBar, "speed", TW_TYPE_DOUBLE, &speed, " label='Rot speed' min=0 max=2 step=0.01 keyIncr=s keyDecr=S help='Rotation speed (turns/second)' ");
	// Add 'bgColor' to 'bar': it is a modifable variable of type TW_TYPE_COLOR3F (3 floats color)
	vec3 oColor(0.0f);
	TwAddVarRW(g_pToolBar, "bgColor", TW_TYPE_COLOR3F, &oColor[0], " label='Background color' ");

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(g_pWindow, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetCursorPos(g_pWindow, g_nWidth / 2, g_nHeight / 2);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	//	glEnable(GL_CULL_FACE);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("shaders/StandardShading.vertexshader", "shaders/StandardShading.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

	// Load the texture
	GLuint Texture = loadDDS("mesh/uvmap.DDS");

	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");

	// Read our .obj file
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	bool res = loadOBJ("mesh/suzanne.obj", vertices, uvs, normals);

	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs, indexed_normals);

	//Create Variables
	std::set <unsigned short> neighbours;
	std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours> queue_vertex;
	std::stack <Step> steps;

	// Load it into a VBO

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

	// Generate a buffer for the indices as well
	GLuint elementbuffer;
	glGenBuffers(1, &elementbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

	// Get a handle for our "LightPosition" uniform
	glUseProgram(programID);
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	// For speed computation
	double lastTime = glfwGetTime();
	double lastSimplification = glfwGetTime();
	int nbFrames = 0;
	bool flag = true;

	do {
		check_gl_error();

		//use the control key to free the mouse
		if (glfwGetKey(g_pWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			nUseMouse = 1;
		else
			nUseMouse = 0;

		// Measure speed
		double currentTime = glfwGetTime();
		nbFrames++;
		if (currentTime - lastTime >= 1.0) { // If last prinf() was more than 1sec ago
											 // printf and reset
			printf("%f ms/frame\n", 1000.0 / double(nbFrames));
			nbFrames = 0;
			lastTime += 1.0;
		}
		
		//Here start my code
		if (glfwGetKey(g_pWindow, GLFW_KEY_L) == GLFW_PRESS) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_F) == GLFW_PRESS) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_P) == GLFW_PRESS) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_R) == GLFW_PRESS && currentTime > lastSimplification + 0.5) {
			lastSimplification = glfwGetTime();
			reducePolygon(indices, indexed_vertices, indexed_uvs, indexed_normals, queue_vertex, neighbours, steps);
		}
		if (glfwGetKey(g_pWindow, GLFW_KEY_S) == GLFW_PRESS && currentTime > lastSimplification + 0.5) {
			lastSimplification = glfwGetTime();
			vertexSplit(indices, indexed_vertices, indexed_uvs, indexed_normals, steps);
		}
	
		if (flag) {
			flag = reducePolygon(indices, indexed_vertices, indexed_uvs, indexed_normals, queue_vertex, neighbours, steps);
		}
		else if (currentTime > lastSimplification + 0.001) {
			lastSimplification = glfwGetTime();
			if (!vertexSplit(indices, indexed_vertices, indexed_uvs, indexed_normals, steps)) flag = true;
		}
		
		// Generate a buffer for the indices as well

		glGenBuffers(1, &elementbuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs(nUseMouse, g_nWidth, g_nHeight);
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader,
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		glm::vec3 lightPos = glm::vec3(4, 4, 4);
		glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to user Texture Unit 0
		glUniform1i(TextureID, 0);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(
			2,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

		// Draw the triangles !
		glDrawElements(
			GL_TRIANGLES,        // mode
			indices.size(),      // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0             // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		// Draw tweak bars
		TwDraw();

		// Swap buffers
		glfwSwapBuffers(g_pWindow);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(g_pWindow, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(g_pWindow) == 0);

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteBuffers(1, &elementbuffer);
	glDeleteProgram(programID);
	glDeleteTextures(1, &Texture);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Terminate AntTweakBar and GLFW
	TwTerminate();
	glfwTerminate();

	return 0;
}

bool reducePolygon(std::vector<unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices,
	std::vector <glm::vec2>& indexed_uvs, std::vector<glm::vec3>& indexed_normals,
	std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>& queue_vertex, std::set <unsigned short>& neighbours,
	std::stack <Step>& steps) {

	if (indices.size() == 3) {
		std::cout << "You can't reduce more, there is only one triangle!\n";
		return false;
	}
	//Save state before modify
	steps.push(Step(indices, indexed_vertices, indexed_uvs, indexed_normals));

	//Reset these variables
	neighbours.clear();
	queue_vertex = std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>();

	//Create priority qyeye based on less neighbours' number
	createPriorityQueue(indices, indexed_vertices, queue_vertex, neighbours);

	//Take the vertex to remove
	Vertex remove_vertex = queue_vertex.top();
	queue_vertex.pop();
	//Take the vertex_substitute;
	Vertex vertex_substitute = takeVertexSubstitute(remove_vertex.neighbours, queue_vertex);

	halfEdgeCollapse(indices, indexed_vertices, indexed_uvs, indexed_normals, remove_vertex.vertex, vertex_substitute.vertex);

	return true;
}

//Create Priority Queue based on less neighbours' number
void createPriorityQueue(std::vector<unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices,
	std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>& queue_vertex, std::set <unsigned short>& neighbours) {

	unsigned short cont = 0;

	for (int i = 0; i < indexed_vertices.size(); i++) {
		cont = 0;
		neighbours.clear();

		for (int j = 0; j < indices.size(); j++) {
			cont++;
			if (indices[j] == i) {
				if (cont == 1) {
					//First vertex in triangle, add the next two to neighbours
					neighbours.insert(indices[j + 1]);
					neighbours.insert(indices[j + 2]);
					j += 2;
				}
				if (cont == 2) {
					//Second vertex in triangle, add the last ant the next one
					neighbours.insert(indices[j - 1]);
					neighbours.insert(indices[j + 1]);
					j++;
				}
				if (cont == 3) {
					//Last vertex in triangle, add the last two
					neighbours.insert(indices[j - 2]);
					neighbours.insert(indices[j - 1]);
				}
				cont = 0;
			}
			if (cont == 3)	cont = 0;
		}
		neighbours.erase(i);
		if (!neighbours.empty()) queue_vertex.push(Vertex(i, neighbours));
	}

}

Vertex takeVertexSubstitute(std::set <unsigned short> neighbours, std::priority_queue<Vertex, std::vector <Vertex>, OrderNeighbours>& queue_vertex) {

	while (!queue_vertex.empty()) {
		Vertex vertex_substitute = queue_vertex.top();
		queue_vertex.pop();
		for (auto i : neighbours) {
			if (vertex_substitute.vertex == i)	return vertex_substitute;
		}
	}

}

void halfEdgeCollapse(std::vector <unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices, std::vector<glm::vec2> indexed_uvs,
	std::vector<glm::vec3>& indexed_normals, unsigned short remove_vertex, unsigned short vertex_substitute) {

	unsigned short cont = 0;

	for (auto i = 0; i < indices.size(); i++) {
		cont++;
		if (indices[i] == remove_vertex && indices.size() > 3) {
			indices[i] = vertex_substitute;

			if (cont == 1) {
				if (indices[i + 1] == vertex_substitute || indices[i + 2] == vertex_substitute) {
					//Remove indices
					indices.erase(indices.begin() + (i + 2));
					indices.erase(indices.begin() + (i + 1));
					indices.erase(indices.begin() + i);

					i--;
					cont = 0;
				}
			}
			if (cont == 2) {
				if (indices[i - 1] == vertex_substitute || indices[i + 1] == vertex_substitute) {
					//Remove indices
					indices.erase(indices.begin() + (i + 1));
					indices.erase(indices.begin() + i);
					indices.erase(indices.begin() + (i - 1));

					i -= 2;
					cont = 0;
				}
			}
			if (cont == 3) {
				if (indices[i - 1] == vertex_substitute || indices[i - 2] == vertex_substitute) {
					//Remove indices
					indices.erase(indices.begin() + i);
					indices.erase(indices.begin() + (i - 1));
					indices.erase(indices.begin() + (i - 2));

					i -= 3;
					cont = 0;
				}
			}
		}
		if (cont == 3)	cont = 0;
	}

}

bool vertexSplit(std::vector<unsigned short>& indices, std::vector<glm::vec3>& indexed_vertices,
	std::vector <glm::vec2>& indexed_uvs, std::vector<glm::vec3>& indexed_normals, std::stack <Step>& steps) {

	if (steps.empty()) {
		std::cout << "You can split more. The image is complete!\n";
		return false;
	}
	Step step = steps.top();
	steps.pop();
	indices.swap(step.indices);
	indexed_vertices.swap(step.indexed_vertices);
	indexed_normals.swap(step.indexed_normals);
	indexed_uvs.swap(step.indexed_uvs);

	return true;
}