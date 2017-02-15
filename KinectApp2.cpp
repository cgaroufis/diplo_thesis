//Version a: 4 possible hand poses (up, stretched front, stretched sides, down)//
//Behavior when hand pose can not be classified as any of these four categories: no sin tone//
//(basically when there is no hand symmetry)//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <string>
#include "../../../../Program Files/Microsoft SDKs/Kinect/v2.0_1409/inc/Kinect.h" //insert your path to the Kinect header there.

#define PI = 3.14159;
#define BUFSIZE  (512) //lost data is[?] lost data. :)
#define PIPENAME "\\\\.\\pipe\\userdata"
#define PIPENAME_A "\\\\.\\pipe\\mode"

using namespace std;

struct DirectionVector
{
	double Xnorm;
	double Ynorm;
	double Znorm;
};

int GetHandState(Joint* jointarr);
DirectionVector CalculateHandPartOrientation(Joint base, Joint effector, char side);
double CalculateFrontalDirection(Joint left, Joint right);
int CalculateSpeed(Joint center);
int ClassifyHandVector(DirectionVector ShoulderToElbow, DirectionVector ElbowToWrist, double Radians);
int GetPositionDescriptor(Joint center);
void GetPoseName(int desc, int user);
void GetPositionName(int desc, int user);

int _tmain()
{
	//Definitions
	int State[6],dc = -1;
	char data[2],c[1];

	//wait for pipe connection with the shell, to pass mode data, and block until received.
	HANDLE hPipeA = CreateFileA(PIPENAME_A, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	do
	{
		while (ReadFile(hPipeA, c, 1, NULL, NULL) == 0);
		dc = c[0] - 48;
	} while (dc < 0);

	//Opening Kinect Sensor interface
	IKinectSensor   * mySensor = nullptr;
	while (GetDefaultKinectSensor(&mySensor) !=S_OK); //locks until a Kinect sensor is detected...
	while(mySensor->Open() != S_OK); //and until kinect is opened.

	//initializations...
	int myBodyCount = 0;
	IBodyFrameSource * myBodySource = nullptr;
	IBodyFrameReader * myBodyReader = nullptr;
	IBodyFrame * myBodyFrame = nullptr;

	//getting needed initial values for body variables
	mySensor->get_BodyFrameSource(&myBodySource);
	myBodySource->OpenReader(&myBodyReader);
	myBodySource->get_BodyCount(&myBodyCount);
	printf("Initialisations completed, entering stream mode\n");
	
	//creating connection to the audio player

	HANDLE hPipe = CreateNamedPipeA(PIPENAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES, BUFSIZE, BUFSIZE, 0, NULL);
	while (ConnectNamedPipe(hPipe, NULL) == 0);
	
	while (1) //would like it to lock only when kinect is running/streaming/...
	{
		while (myBodyReader->AcquireLatestFrame(&myBodyFrame) != S_OK); //locks while waiting for the next frame?
		IBody ** bodyArr = nullptr;
		myBodySource->get_BodyCount(&myBodyCount);
		bodyArr = new IBody *[myBodyCount];

		for (int i = 0; i < myBodyCount; i++)  
			bodyArr[i] = nullptr;

		myBodyFrame->GetAndRefreshBodyData(myBodyCount, bodyArr); //needed to refresh bodyArr data(trackability eg)

		int Speed = 200; //dummy initialisation

		for (int i = 0; i < 2; i++) //makes sure only 2 people are tracked even in cases more are.
		{
			BOOLEAN result = false;
			if (bodyArr[i]->get_IsTracked(&result) == S_OK && result) //skeleton tracking loop, entered only for tracked bodies
			{
				Joint   jointArr[JointType_Count]; //JointType_Count: constant = 25
				bodyArr[i]->GetJoints(JointType_Count, jointArr);
				
				if (dc == 0)
				{
					State[i] = GetPositionDescriptor(jointArr[1]);
					GetPositionName(State[i],i);
				}
				else
				{
					State[i] = GetHandState(jointArr);
					GetPoseName(State[i],i);
				}
				if (i==0) Speed = CalculateSpeed(jointArr[1]); //how do we get the main user (who will set speed via distance)
			

				//printf("Current pose for user %d: %d\n", i, State[i]);
			}
		}

		//Sending all needed data.//
		data[0] = State[0] + 48;
		//data[1] = State[1] + 48;
		data[1] = 55;
		WriteFile(hPipe, data, 2, NULL, NULL);
		
		printf("Current speed: %d\n", Speed);
		Sleep(Speed);

		//release current frame & clear screen
		myBodyFrame->Release();
		system("CLS");
	}

	return 0;
}

int GetHandState(Joint* jointArr)
{
	//calculate viewing direction (in angle form)
	double UpperAngle = CalculateFrontalDirection(jointArr[4], jointArr[8]);
	double LowerAngle = CalculateFrontalDirection(jointArr[12], jointArr[16]);
	double Radians = 0.5*(UpperAngle + LowerAngle);

	//calculate hand directions (in 1-norm vector form)
	DirectionVector UpperLeftHandDir = CalculateHandPartOrientation(jointArr[4], jointArr[5],'l');
	DirectionVector UpperRightHandDir = CalculateHandPartOrientation(jointArr[8], jointArr[9],'r');
	DirectionVector LowerLeftHandDir = CalculateHandPartOrientation(jointArr[5], jointArr[6],'l');
	DirectionVector LowerRightHandDir = CalculateHandPartOrientation(jointArr[9], jointArr[10],'r');

	//assign part orientations using as features the direction vectors.
	int LeftHandDescr = ClassifyHandVector(UpperLeftHandDir,LowerLeftHandDir, -Radians); //returns values in 0-3 range
	int RightHandDescr = ClassifyHandVector(UpperRightHandDir,LowerRightHandDir,Radians);

	//printf("X coordinate of upper left shoulder: %f\n", jointArr[4].Position.X);
	//printf("X coordinate of upper right shoulder: %f\n", jointArr[8].Position.X);

    //for a successful classification, we want both hands to have a similar pose
	if (LeftHandDescr == RightHandDescr) return LeftHandDescr+2; //returns a value between 2 and 5 as descriptor
	else return 0; //0 as descriptor should mean no input.
}

int ClassifyHandVector(DirectionVector upper_vect, DirectionVector lower_vect, double rad)
//Pose descriptors: 0 for raised hands, 3 for forward stretched hands
//2 for sideways stretched hands, 1 for hands down.
{
	double d[4];
	double min = 555; //yolo.
	int idx = 4; //yolo[2].

	//assign distances (in 2 steps)
	d[0] = sqrt(pow(upper_vect.Xnorm, 2) + pow(upper_vect.Ynorm - 1, 2) + pow(upper_vect.Znorm, 2));
	d[0] = d[0] + sqrt(pow(lower_vect.Xnorm, 2) + pow(lower_vect.Ynorm - 1, 2) + pow(lower_vect.Znorm, 2));
	d[1] = sqrt(pow(upper_vect.Xnorm, 2) + pow(upper_vect.Ynorm + 1, 2) + pow(upper_vect.Znorm, 2));
	d[1] = d[1] + sqrt(pow(lower_vect.Xnorm, 2) + pow(lower_vect.Ynorm + 1, 2) + pow(lower_vect.Znorm, 2));
	d[2] = sqrt(pow(upper_vect.Xnorm - cos(rad), 2) + pow(upper_vect.Ynorm, 2) + pow(upper_vect.Znorm - sin(rad), 2));
	d[2] = d[2] + sqrt(pow(lower_vect.Xnorm - cos(rad), 2) + pow(lower_vect.Ynorm, 2) + pow(lower_vect.Znorm - sin(rad), 2));
	d[3] = sqrt(pow(upper_vect.Xnorm - sin(rad), 2) + pow(upper_vect.Ynorm, 2) + pow(upper_vect.Znorm + cos(rad), 2));
	d[3] = d[3] + sqrt(pow(lower_vect.Xnorm - sin(rad), 2) + pow(lower_vect.Ynorm, 2) + pow(lower_vect.Znorm + cos(rad), 2));

	//find the minimum//
	for (int i = 0; i < 4; i++)
	{
		if (d[i] < min)
		{
			min = d[i];
			idx = i;
		}
	}
	
	return idx;
}

DirectionVector CalculateHandPartOrientation(Joint base, Joint effector, char side)
{
	DirectionVector dir;

	double PartLength = sqrt(pow(base.Position.X - effector.Position.X, 2) + pow(base.Position.Y - effector.Position.Y, 2) + pow(base.Position.Z - effector.Position.Z, 2));
	double Xlength = effector.Position.X - base.Position.X;
	double Ylength = effector.Position.Y - base.Position.Y;
	double Zlength = effector.Position.Z - base.Position.Z;

	dir.Xnorm = Xlength / PartLength;
	dir.Ynorm = Ylength / PartLength;
	dir.Znorm = Zlength / PartLength;

	if (side == 'l') dir.Xnorm = - dir.Xnorm;
	return dir;
}

double CalculateFrontalDirection(Joint left, Joint right)
{
	float x_coordinate = (right.Position.X - left.Position.X);
	float z_coordinate = (right.Position.Z - left.Position.Z);
	return atan2(z_coordinate, x_coordinate); //allows the angle to take values in the [-180deg, 180deg] space.
}

int GetPositionDescriptor(Joint center)
{
	bool x_coordinate_d = (center.Position.X > 0); //right - left 
	bool z_coordinate_d = (center.Position.Z > 2.5); //near - far
	if (z_coordinate_d)
	{
		if (x_coordinate_d) return 5;
		else return 4;
	}
	else
	{
		if (x_coordinate_d) return 3;
		else return 2;
	}
}

int CalculateSpeed(Joint center)
{
	float x_distance = center.Position.X;
	float z_distance = center.Position.Z;
	float distance_from_camera = sqrt(pow(x_distance, 2) + pow(z_distance, 2)); //normally takes values between 0.5, 5//
	int temp = int(round(distance_from_camera * 200));
	return temp;
}

void GetPositionName(int desc, int user)
{
	printf("Position for user %d\n", user + 1);
	if (desc == 5) printf("Far right\n");
	else if (desc == 4) printf("Far left\n");
	else if (desc == 3) printf("Close right\n");
	else printf("Close left\n");
}

void GetPoseName(int desc, int user)
{
	printf("Hand pose for user %d\n", user + 1);
	if (desc == 5) printf("Hands stretched sideways\n");
	else if (desc == 4) printf("Hands stretched forward\n");
	else if (desc == 3) printf("Hands down\n");
	else if (desc == 2) printf("Hands up\n");
	else printf("No symmetry\n");
}