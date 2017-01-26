 /*
 * camera.h
 *
 *  Created on: 22Jan.,2017
 *      Author: bcub3d-desktop
 */

#ifndef MAVAIRCRAFT_H_
#define MAVAIRCRAFT_H_

// GL Includes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>
#include <mutex>

#include "model.h"
#include "../src/fonts.h"

// Define Fonts based on OS
#ifdef _WIN32
	#define FONTPATH "C:/Windows/Fonts/Arial.ttf"
#elif __linux__
	#define FONTPATH "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf"
#endif

// Derived Class
class MavAircraft : public Model {
public:
	/* Data */
		// Position Information
		glm::dvec3 			origin; 						// Lat (deg), Lon (deg), alt (km)
		glm::dvec3 			geoPosition; 					// Lat (deg), Lon (deg), alt (km)
		glm::dvec3 			position; 						// (x,y,z) relative to origin
		glm::dvec3 			velocity;						// (vx,vy,vz) (m/s)

		// Position History Information
		vector<glm::dvec3>	geoPositionHistory;				// Vector of Lat (deg), Lon (deg), alt (km)
		vector<glm::dvec3>	positionHistory; 				// Vector of (x,y,z) relative to origin
		vector<glm::dvec3>	velocityHistory;				// Vector of (vx,vy,vz)
		vector<float>		timePositionHistory;			// Vector of floats corresponding to times of position history
		bool				firstPositionMessage = true;	// True if the first message has been received
		unsigned int		currentPosMsgIndex = 0;			// Index of the 'latest' position mavlink message being displayed (this is behind the data)

		// Attitude Information
		glm::dvec3 			attitude;						// roll (rad), pitch (rad), yaw (rad)
		vector<glm::dvec3>	attitudeHistory; 				// Vector if attiudes
		vector<glm::dvec3>	attitudeRateHistory;			// Vector of attitude rates
		vector<float>		timeAttitudeHistory;			// Vector of floats corresponding to times of attitude history
		bool				firstAttitudeMessage = true;	// True if the first message has been recieved
		unsigned int		currentAttMsgIndex = 0; 		// Index of the 'latest' attitude mavlink message being displayed (behind the data)

		// Time Information
		float				timeStart=0;					// Offset between autopilot boot time and glfw time (used to sync times)
		float				timeStartMavlink=0; 			// Boot time of the first mavlink message (s)
		float				timeDelay=0.5;  				// Delay between receiving the first mavlink message and displaying it (s)
		float				currTime=0;						// The current time
		float				dtPos=0;						// Timestep between current frame and last current position mavlink message time
		float				dtAtt=0;						// Timestep between current frame and last current attitude mavlink message time

		// Interpolation Information
		glm::dvec3 			xPosConst;
		glm::dvec3 			yPosConst;
		glm::dvec3 			zPosConst;

		// Attitude Information

		// Frame Information
		//GLfloat fovX; // Degrees
		//GLfloat fovY; // Degrees

		// Alert Font
		GLFont* alertFontPt;
		Shader* alertShaderPt;
		GLfloat screenHeight = 1080;
		GLfloat screenWidtPosh  = 1920;

	MavAircraft(GLchar* path, glm::dvec3 origin,GLFont* alertFontPt,Shader* alertShaderPt) : Model(path) {

		// Set Geoposition (temporary)
		this->geoPosition = glm::dvec3(-37.958926f, 145.238343f, 0.0f);
		this->geoPositionHistory.push_back(this->geoPosition);

		// Set Origin
		this->origin = origin;

		// Font information
		this->alertFontPt = alertFontPt;
		this->alertShaderPt = alertShaderPt;

	}

	void Draw(Shader shader) {

		// Set new time
		currTime = glfwGetTime() - timeStart;

		// Check to move to next pair of position messages
		if(!firstPositionMessage) {
			while(currTime > (timePositionHistory[currentPosMsgIndex]-timeStartMavlink)) {
				if (currentPosMsgIndex < timePositionHistory.size()-1) {
					// Increment current Mavlink Message
					currentPosMsgIndex += 1;
				}
			}
			if (currentPosMsgIndex>0) {
				currentPosMsgIndex -= 1;
			}
		}

		// Check to move to the next pair of attitude messages
		if(!firstAttitudeMessage) {
			while(currTime > timeAttitudeHistory[currentAttMsgIndex]-timeStartMavlink) {
				if(currentAttMsgIndex < timeAttitudeHistory.size()-1) {
					// Increment current Mavlink Message
					currentAttMsgIndex += 1;
				}
			}
			if(currentAttMsgIndex>0) {
				currentAttMsgIndex -= 1;
			}
		}

		if(currentPosMsgIndex>1) {
			// Calculate position offset
			if (timePositionHistory.size() > 0) {
				if(!firstPositionMessage) {
					dtPos = currTime - (timePositionHistory[currentPosMsgIndex]-timeStartMavlink);
					interpolatePosition();
				}
			}

			// Calculate attitude offset
			if(timeAttitudeHistory.size() > 0) {
				if(!firstAttitudeMessage) {
					dtAtt = currTime - (timeAttitudeHistory[currentAttMsgIndex]-timeStartMavlink);
				}
			}

			// Alert to screen if running ahead of mavlink messages
			if(currTime > timePositionHistory[timePositionHistory.size()-1]) {
				std::stringstream ss;
				ss << "Messages falling behind: " << -dtPos << " s";
				alertFontPt->RenderText(alertShaderPt,ss.str(),(screenWidtPosh/2.0)-400.0,screenHeight/10.0f,1.0f,glm::vec3(0.0f, 1.0f, 0.0f));
			}

			// Do Translation and Rotation
			glm::mat4 model;
			model = glm::translate(model,glm::vec3(position[0],position[2],position[1]));// Translate first due to GLM ordering, rotations opposite order
			model = glm::rotate(model,(float)attitude[0],glm::vec3(1.0f,0.0f,0.0f)); // Rotate about x, roll
			model = glm::rotate(model,(float)attitude[1],glm::vec3(0.0f,0.0f,1.0f)); // Rotate about z, pitch
			model = glm::rotate(model,(float)attitude[2],glm::vec3(0.0f,1.0f,0.0f)); // Rotate about y, yaw


			// Update Uniforms
			glUniformMatrix4fv(glGetUniformLocation(shader.Program,"model"),1,GL_FALSE,glm::value_ptr(model));

			// Draw Model
			Model::Draw(shader);
		}
	}

	// Calculate position at next frame
	void interpolatePosition() {
		if(currentPosMsgIndex>1) {
			// Recalculate Interpolation Constants
			if(currentPosMsgIndex>1) {
				calculateInterpolationConstants();
			}

			// Calculate Positions
			this->position[0] = (0.5*xPosConst[0]*dtPos*dtPos) + (xPosConst[1]*dtPos) + xPosConst[2];
			this->position[1] = (0.5*yPosConst[0]*dtPos*dtPos) + (yPosConst[1]*dtPos) + yPosConst[2];
			this->position[2] = (0.5*zPosConst[0]*dtPos*dtPos) + (zPosConst[1]*dtPos) + zPosConst[2];
		}
	}

	void calculateInterpolationConstants() {
		// Get Index
		int pos = currentPosMsgIndex + 1;

		// x(t) = 0.5*a*t^2+b*t+c
		// v(t) = at+b
		// (t1,x1), (t2,x2), (t1,v1), v1 found from last frame step
		float t1 = 0;
		float t2 = timePositionHistory[pos]-timePositionHistory[pos-1];
		glm::dvec3 v1 = velocityHistory[pos];

		// Find inverse matrix of [x1,x2,v1]=[BLAH][a,b,c]
		glm::mat3x3 A = glm::mat3x3(0.5*t1*t1, t1, 1,0.5*t2*t2,t2,1,t1,1,0);
		glm::mat3x3 inv = glm::inverse(A);

		// Constants
		glm::dvec3 xvec = glm::dvec3(positionHistory[pos-1][0],positionHistory[pos][0],v1[0]);
		glm::dvec3 yvec = glm::dvec3(positionHistory[pos-1][1],positionHistory[pos][1],v1[1]);
		glm::dvec3 zvec = glm::dvec3(positionHistory[pos-1][2],positionHistory[pos][2],v1[2]);

		xPosConst = xvec*inv;		// Flipped due to GLM ordering
		yPosConst = yvec*inv;		// Flipped due to GLM ordering
		zPosConst = zvec*inv;		// Flipped due to GLM ordering
	}


	/* Conversion Geodetic to ECEF */
	glm::dvec3 geo2ECEF(glm::dvec3 positionVector) {
		// positionVector: (latitude, longitude, altitude (m))
		// Uses WGS84 defined here https://en.wikipedia.org/wiki/Geodetic_datum#Geodetic_to.2Ffrom_ECEF_coordinates
		const double pi = 3.141592653589793238462643383279502884;
		GLdouble a = 6378137.0;
		GLdouble e2 = 6.6943799901377997e-3;
		GLdouble lat = positionVector[0] * pi /180.0;
		GLdouble lon = positionVector[1] * pi /180.0;
		GLdouble alt = positionVector[2];
		GLdouble N = a / glm::sqrt(1-(e2*glm::sin(lat)*glm::sin(lat)));
		GLdouble ex = (N+alt)*glm::cos(lat)*glm::cos(lon); // m
		GLdouble ey = (N+alt)*glm::cos(lat)*glm::sin(lon); // m
		GLdouble ez = (N*(1-e2) + alt) * glm::sin(lat);    // m

		return glm::dvec3(ex,ey,ez);
	}

	/* Convert from ECEF to NEU */
	glm::dvec3 ecef2NEU(glm::dvec3 ecefVector, glm::dvec3 ecefOrigin, glm::dvec3 origin) {
		GLdouble lat = glm::radians(origin[0]);
		GLdouble lon = glm::radians(origin[1]);
		glm::dmat3 A = glm::dmat3(-glm::sin(lat)*glm::cos(lon),	-glm::sin(lat)*glm::sin(lon),	glm::cos(lat),
								  -glm::sin(lon),				glm::cos(lon),					0.0,
								  glm::cos(lat)*glm::cos(lon),	glm::cos(lat)*glm::sin(lon),	glm::sin(lat));
		glm::dvec3 B = glm::dvec3(ecefVector[0]-ecefOrigin[0],ecefVector[1]-ecefOrigin[1],ecefVector[2]-ecefOrigin[2]);

		return B*A; // Flipped due to GLM ordering
	}

};



#endif /* MAVAIRCRAFT_H_ */
