/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file piMaster_dynamoMesh_temp.cpp
 * @objective Creates Raspberry Pi as master node in a mesh network, receiving temp data from nodes and then stores data in DyanomoDB.
 *
 * This example takes the parameters from the aws_iot_config.h file and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to a variable topic based on the received nodeID from node's payload - "Sensor/temp/<nodeID>"
 *
 * If all the certs are correct, you should see the messages received by the application in a loop.
 * The application takes in the certificate path, host name , port and the number of times the publish should happen.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

/* nRF24L01 radio Networking  */
#include "RF24Mesh/RF24Mesh.h"
#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>

/** USER CONFIG: RPi nRF24L01 PINOUT 
* nRF24L01 radio pin setup on RPi in order of CE, CSN and SPI speed 
* Setup for GPIO 22 CE and CE1 CSN with SPI speed @ 8 MHz
*/
RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

RF24Network network(radio);
RF24Mesh mesh(radio,network);

/** USER CONFIG: NODE PAYLOAD
* The payload received by mesh nodes
* The arduino node example has a replica of this structure
* nodeload_t can easily be adopted to different payload data sets
*/ 
typedef struct nodeload_t {
	unsigned long temp;
	unsigned long id;
} nodeload_t;

/** USER CONFIG: TOPIC
* topic is referred to in publishing with a nodeID from nodeload appended.
* topic changes made here are consistent throughout.
*/
 char topic[] = "Sensor/temp/";

/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "aws-iot-device-sdk-embedded-C-master/certs";

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 0;

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	IOT_INFO("Subscribe callback");
	IOT_INFO("%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, params->payload);
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} else {
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

void parseInputArgsForConnectParams(int argc, char **argv) {
	int opt;

	while(-1 != (opt = getopt(argc, argv, "h:p:c:x:"))) {
		switch(opt) {
			case 'h':
				strcpy(HostAddress, optarg);
				IOT_DEBUG("Host %s", optarg);
				break;
			case 'p':
				port = atoi(optarg);
				IOT_DEBUG("arg %s", optarg);
				break;
			case 'c':
				strcpy(certDirectory, optarg);
				IOT_DEBUG("cert root directory %s", optarg);
				break;
			case 'x':
				publishCount = atoi(optarg);
				IOT_DEBUG("publish %s times\n", optarg);
				break;
			case '?':
				if(optopt == 'c') {
					IOT_ERROR("Option -%c requires an argument.", optopt);
				} else if(isprint(optopt)) {
					IOT_WARN("Unknown option `-%c'.", optopt);
				} else {
					IOT_WARN("Unknown option character `\\x%x'.", optopt);
				}
				break;
			default:
				IOT_ERROR("Error in command line argument parsing");
				break;
		}
	}

}

int main(int argc, char **argv) {

	/* The nodeID is set to 0 for the master node */
        mesh.setNodeID(0);
        /* Connect to the mesh */
        IOT_INFO("Starting nRF24L01 Radio Mesh\n");
        mesh.begin();
	radio.printDetails();
	
	bool infinitePublishFlag = true;

	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cPayload[100];

	IoT_Error_t rc = FAILURE;

	AWS_IoT_Client client;
	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;
	IoT_Publish_Message_Params paramsQOS0;

	parseInputArgsForConnectParams(argc, argv);

	IOT_INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	getcwd(CurrentWD, sizeof(CurrentWD));
	snprintf(rootCA, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_ROOT_CA_FILENAME);
	snprintf(clientCRT, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_CERTIFICATE_FILENAME);
	snprintf(clientKey, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_PRIVATE_KEY_FILENAME);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = HostAddress;
	mqttInitParams.port = port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 10;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = (char *)AWS_IOT_MQTT_CLIENT_ID;
	connectParams.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	connectParams.isWillMsgPresent = false;

	IOT_INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&client, &connectParams);
	if(SUCCESS != rc) {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
		return rc;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}
	
	/*
	* Subscribing to topicSub 
	* aws_iot_mqtt_subscribe(&(<IoT Client instance>), topic name, length of topic name, QoS, subscribe callback handler
	*					       ,optional pointer to some data to be returned to the subscribe handler)
	* topicSub has "+" appended for subscribing to Sensor/topic/<every_nodeID>
	* MQTT Wildcards: "+", "#", and "*"
	*/
	char topicSub[14];
	snprintf(topicSub,14,"%s+", topic);
	IOT_INFO("Subscribing...");
	rc = aws_iot_mqtt_subscribe(&client, topicSub, strlen(topicSub), QOS0, iot_subscribe_callback_handler, NULL);
	
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d ", rc);
		return rc;
	}
	
	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;

	if(publishCount != 0) {
		infinitePublishFlag = false;
	}


	while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)
		  && (publishCount > 0 || infinitePublishFlag)) {
		/*
		* If network.available() is false, then the following while loop is exited in order to update radio network
		*/
		mesh.update();
		mesh.DHCP();
                        
		while(network.available()) {
			/*
			* Max time the yield function will wait for read messages
			*/
			rc = aws_iot_mqtt_yield(&client, 100);

			if(NETWORK_ATTEMPTING_RECONNECT == rc) {
				/*
				* If the client is attempting to reconnect we will skip the rest of the loop.
				*/
				continue;
			}

			IOT_INFO("-->sleep");
			sleep(1);

			RF24NetworkHeader header;
                        network.peek(header);
			nodeload_t nodeload;
			/*
			* header.type 'M' is standard in arduino node code
			* if there is an error in sending node payload then switch() will default
			*/
                        switch(header.type) {
                        case 'M': network.read(header,&nodeload,sizeof(nodeload));
                                /*
				* Stores node payload, "nodeload", data into cPayload
				* cPayload is formatted to be accepted in AWS dynamoDB
				* Data entering as string: "\"%s\": \"%lu\""
				* Data entering as number: "\"%s\": %lu"
				*/
				sprintf(cPayload,
					"{"
					    "\"%s\": %lu"
					    ","
				            "\"%s\": %lu"
                                    
					"}", 
					"temperature", nodeload.temp,
					"nodeID", nodeload.id);
				paramsQOS0.payloadLen = strlen(cPayload);
		
				/*
				* Publish to 'Sensor/temp/<nodeload.id>'
				* aws_iot_mqtt_publish(&iotClient, topicName, topicNameLength, &msgParams)
				*/
				char topicPub[16];
                                snprintf(topicPub, 16, "%s%lu", topic, nodeload.id);
				rc = aws_iot_mqtt_publish(&client, topicPub, strlen(topicPub), &paramsQOS0);
                                break;

                        default:  network.read(header,0,0);
                                printf("Rcv bad type %d from 0%o\n",header.type,header.from_node);
                                break;
                        }	

			if(publishCount > 0) {
				publishCount--;
			}
		}
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop.\n");
	} else {
		IOT_INFO("Publish done\n");
	}

	return rc;

}
