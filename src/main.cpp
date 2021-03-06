/*
 * main.cpp
 *
 *  Created on: 1Jan.,2017
 *      Author: bcub3d-desktop
 *
 *  Based on tutorials from http://www.learnopengl.com/#!Introduction
 */

// Standard Includes
#include <string>
#include <unistd.h>
#include <memory>
#include <chrono>
#include <thread>

// openGLPlotLive Includes
#include "../openGLPlotLive/src/fonts.h"
#include "../openGLPlotLive/src/line2d.h"
#include "../openGLPlotLive/src/plot.h"
#include "../openGLPlotLive/src/window.h"

// openGL Includes
#include "window.h"
#include "settings.h"
#include "shader.h"
#include "model.h"
#include "fonts.h"
#include "light.h"
#include "imageTile.h"
#include "mavlinkReceive.h"
#include "mavAircraft.h"
#include "skybox.h"
#include "telemOverlay.h"
#include "satTiles.h"
#include "volumes.h"

// GLM Mathematics
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// SOIL Image loading library
#include <SOIL.h>

// Free Type Library
#include <ft2build.h>
#include FT_FREETYPE_H

using std::vector;

// Define Fonts based on OS
#ifdef _WIN32
	#define FONTPATH "C:/Windows/Fonts/Arial.ttf"
#elif __linux__
	#define FONTPATH "/usr/share/fonts/truetype/abyssinica/AbyssinicaSIL-R.ttf"
#endif

// File check time tracking
GLfloat fileChecklast = 0.0f;



int main(int argc, char* argv[]) {
	/* ======================================================
	 *                      Load Settings
	   ====================================================== */
	Settings settings("../Configs/currentConfig.txt");

	/* ======================================================
	 *                     Setup Window
	   ====================================================== */
	// Init GLFW
	GLFWwindow* window = initGLFW(&settings);

	// Initialise GLEW - setup OpenGL pointers
	initGLEW();

	/* ======================================================
	 *                  Command Line Arguments
	   ====================================================== */
	// Parse Command Line Arguments
	bool wireFrameOn = false;
	bool fpsOn = false;
	int opt;
	while((opt = getopt(argc, argv, "wf")) != -1) {
		switch(opt) {
		case 'w': wireFrameOn = true; break;
		case 'f': fpsOn = true; break;
		}
	}

	// Wireframe command line argument
	if(wireFrameOn) {
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	}

	/* ======================================================
	 *                  Create Loading Screen
	   ====================================================== */
	LoadingScreen loadingScreen(window, &settings);

	/* ======================================================
	 *                  	  Shaders
	   ====================================================== */
	// Setup and compile shaders
	loadingScreen.appendLoadingMessage("Loading lightingShader.");
	Shader lightingShader("../Shaders/multiple_lighting.vs","../Shaders/multiple_lighting.frag");
	loadingScreen.appendLoadingMessage("Loading tileShader.");
	Shader tileShader("../Shaders/tileImage.vs","../Shaders/tileImage.frag");
	loadingScreen.appendLoadingMessage("Loading skyboxShader.");
	Shader skyboxShader("../Shaders/skybox.vs","../Shaders/skybox.frag");
	loadingScreen.appendLoadingMessage("Loading simpleShader.");
	Shader simpleShader("../Shaders/telemOverlay.vs","../Shaders/telemOverlay.frag");
	loadingScreen.appendLoadingMessage("Loading volumeShader.");
	Shader volumeShader("../Shaders/volume.vs","../Shaders/volume.frag");
	loadingScreen.appendLoadingMessage("Loading lineShader.");
	Shader lineShader("../Shaders/line.vs","../Shaders/line.frag");

	/* Colours */
	std::vector<glm::vec3> colorVec = {LC_BLUE, LC_RED, LC_GREEN, LC_YELLOW, LC_CYAN, LC_MAGENTA, LC_SILVER, LC_GRAY, LC_MAROON, LC_OLIVE, LC_DARKGREEN, LC_PURPLE, LC_TEAL, LC_NAVY};

	/* ======================================================
	 *                         Fonts
	   ====================================================== */
	// Load Font Shader
	loadingScreen.appendLoadingMessage("Loading textShader.");
	Shader textShader = setupFontShader("../Shaders/font.vs", "../Shaders/font.frag",&settings);

	// Load Telemetry Font
	GLFont telemFont = GLFont(FONTPATH);

	// Load Font
	GLFont* fpsFontPt;

	Shader* textShaderPt = &textShader;
	if(fpsOn) {
		fpsFontPt = new GLFont(FONTPATH);
	}

	// Help Font
	loadingScreen.appendLoadingMessage("Loading fonts.");
	GLFont helpFont = GLFont(FONTPATH);


	/* ======================================================
	 *                        Models
	   ====================================================== */
	glm::vec3 worldOrigin = glm::vec3(settings.origin[0], settings.origin[1], settings.origin[2]/1000.0);
	int num = settings.aircraftConList.size();
	std::vector<MavAircraft> mavAircraftList;
	mavAircraftList.reserve(num);
	std::vector<MavSocket> mavSocketList;
	mavSocketList.reserve(num);
	std::vector<std::thread*> threadList;
	threadList.reserve(num);
	std::thread* threadPt = NULL;
	std::vector<TelemOverlay> telemOverlayList;
	telemOverlayList.reserve(num);
	// Load Mavlink Aircraft
	for(unsigned int i=0; i<settings.aircraftConList.size(); i++) {
		loadingScreen.appendLoadingMessage("Loading mavAircraft: " + settings.aircraftConList[i].name);
		// Load Models
		mavAircraftList.push_back(MavAircraft(settings.aircraftConList[i].filepath.c_str(),worldOrigin,settings.aircraftConList[i].name));
		// Create thread to receive Mavlink messages
		loadingScreen.appendLoadingMessage("Creating mavSocket: " + settings.aircraftConList[i].name);
		mavSocketList.push_back(MavSocket(settings.aircraftConList[i].ipString, settings.aircraftConList[i].port, &mavAircraftList[i]));
		threadPt = new std::thread(&MavSocket::startSocket,&mavSocketList[i]);
		threadList.push_back(threadPt);
		// Create Telem Overlay
		loadingScreen.appendLoadingMessage("Loading telemetry overlay: " + settings.aircraftConList[i].name);
		telemOverlayList.push_back(TelemOverlay(&mavAircraftList[i],&textShader,&telemFont,colorVec[i],&settings));
	}


	// Create Skybox
	loadingScreen.appendLoadingMessage("Loading skybox.");
	vector<const GLchar*> faces;
	faces.push_back("../Models/skybox/right.png");
	faces.push_back("../Models/skybox/left.png");
	faces.push_back("../Models/skybox/top.png");
	faces.push_back("../Models/skybox/bottom.png");
	faces.push_back("../Models/skybox/back.png");
	faces.push_back("../Models/skybox/front.png");
	Skybox skybox(faces);

	/* ======================================================
	 *                       Overlays
	   ====================================================== */
	// Create Origin
	glm::vec3 origin = worldOrigin;

	// Create Tiles
	GLfloat fovX = 48.3/2.0;
	GLfloat fovY = 36.8/2.0;
	TileList imageTileList(origin, fovX, fovY, window);
	// Get Tile Information
	imageTileList.updateTileList("../ImageData",&loadingScreen);

	// Create Satellite Tiles
	SatTileList satTileList(origin,&mavAircraftList[0]);

	/* ======================================================
	 *                        Volumes
	   ====================================================== */
	// Create Volumes
	//std::vector<Volume> volumeList;
	VolumeList volumeList(&camera);
	for(unsigned int i=0; i<settings.volumeList.size(); i++) {
		// Create Volume
		volumeList.addVolume(Volume(origin, settings.volumeList[i]));
	}


	/* ======================================================
	 *                         Lights
	   ====================================================== */
	loadingScreen.appendLoadingMessage("Loading Lights.");
	// Load Lights
	DirectionalLight myDirLight({-0.2,-1.0,-0.3}, {0.5,0.5,0.5}, {0.4,0.4,0.4}, {0.5,0.5,0.5}, &lightingShader,0);

	// Set number of lights
	glUniform1i(glGetUniformLocation(lightingShader.Program,"numLights.nDirLight"),1);
	glUniform1i(glGetUniformLocation(lightingShader.Program,"numLights.nPointLight"),0);
	glUniform1i(glGetUniformLocation(lightingShader.Program,"numLights.nSpotLight"),0);

	/* ======================================================
	 *                     Plotting Data
	   ====================================================== */
	// Create Window Dimensions Class
	GLPL::WinDimensions winDim(window);
	// Setup Shader
	GLPL::Shader plot2dShader("../openGLPlotLive/Shaders/plot2d.vs","../openGLPlotLive/Shaders/plot2d.frag");
	// Create Plot
	GLPL::Plot myplot(0.75, 0.0, 0.25, 0.25, &winDim);
	// Create Line
	// Position
	/*MavAircraft* plotAircraftPt = &mavAircraftList[0];
	GLPL::Line2DVecfVecGLMV3 pos1(&(plotAircraftPt->tempTime),&(plotAircraftPt->tempPos),0);
	pos1.colour = LC_BLUE;
	GLPL::Line2DVecfVecGLMV3 pos2(&(plotAircraftPt->tempTime),&(plotAircraftPt->tempPos),1);
	pos2.colour = LC_BLUE;
	GLPL::Line2DVecfVecGLMV3 pos3(&(plotAircraftPt->tempTime),&(plotAircraftPt->tempPos),2);
	pos3.colour = LC_BLUE;
	// Real Position
	GLPL::Line2DVecfVecGLMV3 rpos1(&(plotAircraftPt->timePositionHistory),&(plotAircraftPt->positionHistory),0,GL_POINTS);
	GLPL::Line2DVecfVecGLMV3 rpos2(&(plotAircraftPt->timePositionHistory),&(plotAircraftPt->positionHistory),1,GL_POINTS);
	GLPL::Line2DVecfVecGLMV3 rpos3(&(plotAircraftPt->timePositionHistory),&(plotAircraftPt->positionHistory),2,GL_POINTS);
	// Attitude
	GLPL::Line2DVecfVecGLMV3 att1(&(plotAircraftPt->tempTime2),&(plotAircraftPt->tempAtt),0);
	att1.colour = LC_RED;
	GLPL::Line2DVecfVecGLMV3 att2(&(plotAircraftPt->tempTime2),&(plotAircraftPt->tempAtt),1);
	att2.colour = LC_RED;
	GLPL::Line2DVecfVecGLMV3 att3(&(plotAircraftPt->tempTime2),&(plotAircraftPt->tempAtt),2);
	att3.colour = LC_RED;
	GLPL::Line2DVecfVecGLMV3 ratt1(&(plotAircraftPt->timeAttitudeHistory),&(plotAircraftPt->attitudeHistory),0,GL_POINTS);
	GLPL::Line2DVecfVecGLMV3 ratt2(&(plotAircraftPt->timeAttitudeHistory),&(plotAircraftPt->attitudeHistory),1,GL_POINTS);
	GLPL::Line2DVecfVecGLMV3 ratt3(&(plotAircraftPt->timeAttitudeHistory),&(plotAircraftPt->attitudeHistory),2,GL_POINTS);
	// Velocity
	GLPL::Line2DVecfVecGLMV3 vel1(&(plotAircraftPt->tempTime),&(plotAircraftPt->tempVel),0);
	vel1.colour = LC_GREEN;
	GLPL::Line2DVecfVecGLMV3 vel2(&(plotAircraftPt->tempTime),&(plotAircraftPt->tempVel),1);
	vel2.colour = LC_GREEN;
	GLPL::Line2DVecfVecGLMV3 vel3(&(plotAircraftPt->tempTime),&(plotAircraftPt->tempVel),2);
	vel3.colour = LC_GREEN;*/

	// Path Plotting
	num = settings.aircraftConList.size();
	std::vector<GLPL::Line2DVecGLMV3> mapList;
	mapList.reserve(num);
	for(unsigned int i=0; i<settings.aircraftConList.size(); i++) {
		mapList.push_back(GLPL::Line2DVecGLMV3(&(mavAircraftList[i].positionHistory),1,0));
		mapList[i].colour = colorVec[i];
		myplot.axes.addLine(&mapList[i]);
	}

	// Add line to axes
//	myplot.axes.addLine(&rpos1);
//	myplot.axes.addLine(&rpos2);
//	myplot.axes.addLine(&rpos3);
//	myplot.axes.addLine(&pos1);
//	myplot.axes.addLine(&pos2);
//	myplot.axes.addLine(&pos3);
//	myplot.axes.addLine(&ratt1);
//	myplot.axes.addLine(&ratt2);
//	myplot.axes.addLine(&ratt3);
//	myplot.axes.addLine(&att1);
//	myplot.axes.addLine(&att2);
//	myplot.axes.addLine(&att3);
//	myplot.axes.addLine(&vel1);
//	myplot.axes.addLine(&vel2);
//	myplot.axes.addLine(&vel3);

	myplot.axes.autoScaleRound = false;
	myplot.axes.equalAxes = true;
	//myplot.axes.maxXRange = 10.0;

	/* ======================================================
	 *                     Drawing Loop
	   ====================================================== */
	// Game Loop
	while(!glfwWindowShouldClose(window)) {
		// Set Frame Time
		GLfloat currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// Check Events
		glfwPollEvents();

		// Update Aircraft Position
		for(unsigned int i=0; i<mavAircraftList.size(); i++) {
			mavAircraftList[i].updatePositionAttitude();
		}

		// Do keyboard movement
		do_movement();

		// Clear the colour buffer
		glClearColor(0.64f, 0.64f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		lightingShader.Use();

		// Update View
		camera.setupView(&mavAircraftList);

		// Update View Position Uniform
		GLint viewPosLoc = glGetUniformLocation(lightingShader.Program, "viewPos");
        glUniform3f(viewPosLoc, camera.Position.x, camera.Position.y, camera.Position.z);

		// Transformation Matrices
        int screenWidth = settings.xRes;
        int screenHeight = settings.yRes;
		glm::mat4 projection = glm::perspective(camera.Zoom, (float)screenWidth/(float)screenHeight,0.1f,10000.0f);
		glm::mat4 view = camera.GetViewMatrix();
		glUniformMatrix4fv(glGetUniformLocation(lightingShader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(lightingShader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));


		// Draw Model
		for(unsigned int i=0; i<mavAircraftList.size(); i++) {
			mavAircraftList[i].Draw(lightingShader);
		}


		// Draw telem overlay
		simpleShader.Use();
		glUniformMatrix4fv(glGetUniformLocation(simpleShader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(simpleShader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));
		for(unsigned int i=0; i<telemOverlayList.size(); i++) {
			telemOverlayList[i].Draw(simpleShader, projection, view, &camera);
		}

		// Draw tile(s)
		tileShader.Use();
		glUniformMatrix4fv(glGetUniformLocation(tileShader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(tileShader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));
		// Check for new files
		if ((currentFrame - fileChecklast) > 0.3) {
			// Update image file list
			//imageTileList.updateTileList("../ImageData",&loadingScreen);
			// Load satellite tiles
			satTileList.updateTiles();
			// Update file check time
			fileChecklast = currentFrame;
		}



		// Draw tiles
		imageTileList.Draw(tileShader);
		// Draw Satellite tiles
		satTileList.Draw(tileShader);

		// Draw Volumes
		volumeShader.Use();
		glUniformMatrix4fv(glGetUniformLocation(volumeShader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(volumeShader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));
		/*for(unsigned int i=0; i<settings.volumeList.size(); i++) {
			volumeList[i].Draw(volumeShader);
		}*/
		volumeList.Draw(volumeShader);
		lineShader.Use();
		glUniformMatrix4fv(glGetUniformLocation(lineShader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(lineShader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));
		/*for(unsigned int i=0; i<settings.volumeList.size(); i++) {
			volumeList[i].DrawLines(lineShader);
		}*/
		volumeList.DrawLines(lineShader);


		// Draw Skybox last
		skyboxShader.Use();
		view = glm::mat4(glm::mat3(camera.GetViewMatrix()));	// Remove any translation component of the view matrix
		glUniformMatrix4fv(glGetUniformLocation(skyboxShader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(skyboxShader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniform1i(glGetUniformLocation(skyboxShader.Program, "skybox"), 0);
		skybox.Draw(skyboxShader);

		// Draw Plot
		GLPL::preLoopDraw(false,&winDim);
		/*rpos1.updateInternalData();
		rpos2.updateInternalData();
		rpos3.updateInternalData();
		pos1.updateInternalData();
		pos2.updateInternalData();
		pos3.updateInternalData();
		ratt1.updateInternalData();
		ratt2.updateInternalData();
		ratt3.updateInternalData();
		att1.updateInternalData();
		att2.updateInternalData();
		att3.updateInternalData();
		vel1.updateInternalData();
		vel2.updateInternalData();
		vel3.updateInternalData();*/
		for(unsigned int i=0; i<settings.aircraftConList.size(); i++) {
			mapList[i].updateInternalData();
		}
		myplot.Draw(plot2dShader);


		// Draw Airspeed
		//telemOverlay.DrawAirspeed();

		// Print FPS
		if(fpsOn) {
			std::stringstream ss;
			ss << int(1.0f/deltaTime) << " fps";
			fpsFontPt->RenderText(textShaderPt,ss.str(),screenWidth-150.0f,screenHeight-50.0f,1.0f,glm::vec3(0.0f, 1.0f, 0.0f),0);
			//std::cout << (1.0f/deltaTime) << "fps" << "\r";
		}

		// Overlay Help Menu
		//std::cout << toggleKeys[GLFW_KEY_H] << '\n';
		if(toggleKeys[GLFW_KEY_H]) {
			std::stringstream sh;
			// Note: Text lines rendered from bottom to top
			sh << "HELP MENU\n";
			sh << "Toggle Help:              h\n";
			sh << "Increment view:           v-b\n";
			sh << "Increment aircraft:       z-x\n";
			sh << "Increment track view:     n\n";
			sh << "Toggle Mouse Movement:  p\n";
			(&helpFont)->RenderText(textShaderPt,sh.str(),0.0f,0.05f,1.0f,glm::vec3(1.0f, 1.0f, 0.0f),1);
		}

		// Swap buffers
		glfwSwapBuffers(window);

		// Sleep to lower framerate
		//std::this_thread::sleep_for(std::chrono::milliseconds(int(1000.0/5.0)));

	}

	glfwTerminate();
	// Close mavlink socket
	for(unsigned int i=0; i<mavSocketList.size(); i++) {
		mavSocketList[i].closeSocket();
		threadList[i]->join();
	}

	// Stop Satellite Tile Threads
	satTileList.stopThreads();

	return 0;
}




