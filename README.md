
Overview

Raspberry Pi variants are low cost options to act as a master node in mesh networking and a gateway to Amazon Web Services(AWS).
Mesh networking allows sensor nodes that may not support AWS authentication to route their payload data seamlessly through other
sensor nodes until reaching the master/gateway that supports AWS authentication.  

The benefit of this is that the combination of a nRF24L01 radio and arduino or other embedded platform is typically less money 
than a Raspberry Pi or other Linux driven device that allows AWS authentication.

The trade off is that the data transmitted with a nRF24L01/low cost microcontroller might be intercepted locally on the way to the master node.
Further work on encryption is a priority.

Design Goals



-------------------------------------------------------------------------------------------
piMaster_dynamoMesh_temp

This basic prgram connects a Raspberry Pi (tested with Pi Zero Wi-Fi) to AWS IoT, AWS dynamoDB and a local mesh network. The mesh network consists of
arduinos with "dhtNode_dynamoMesh_temp.ino" code and hardware setup of a DHT11 temperature/humidity sensor and nRF24L01 2.4 GHz radio. The nodes route their
data payload to the Raspberry Pi which is connected to AWS IoT platform using MQTT. MQTT enables subscribing/publishing to a topic, such as "Sensor/temp/<nodeID>".
A rule made on AWS IoT can perform specified actions based on messages published to a topic.  In this case, a rule was made for the topic "Sensor/temp/<nodeID>" to
store the publish message (node payload) in dynamoDB (NoSQL Cloud Database Service).


##Getting Started

##Steps:

###Setup directories on Raspberry Pi

####AWS IOT Embedded C SDK
 * Create a base directory to hold files e.g (/home/<user>/aws_iot_mesh)
 * Change directory to this new directory
 * Download or clone pi_iotAWS_rf24Mesh
    * Download: wget https://github.com/aaronjense/pi_iotAWS_rf24Mesh/archive/master.zip
    * Clone: git clone https://github.com/aaronjense/pi_iotAWS_rf24Mesh.git
 * Download and Expand the AWS IoT embedded C SDK
	* Download:       wget https://github.com/aws/aws-iot-device-sdk-embedded-C/archive/master.zip
	* Unzip: unzip master.zip 
 * Download external_libs files for AWS IoT embedded C SDK
	* Change Directory:  /aws-iot-device-sdk-embedded-C-master/external_libs/CppUTest
	* View the README.txt that has link for cppUTest github download or Download by: wget https://github.com/cpputest/cpputest/archive/v3.6.zip
	   * unzip v3.6.zip
	   * cd v3.6
           * ./configure and once done make and make check. Further instructions in v3.6/README.md
        * Change Directory: /aws-iot-device-sdk-embedded-C-master/external_libs/mbedTLS
	* View the README.txt that has link for mbedtls-2.1.1  github download or Download by: wget https://github.com/ARMmbed/mbedtls/archive/mbedtls-2.1.1.zip
           * unzip mbedtls-2.1.1.zip
	   * cd mbedtls-2.1.1
	   * make and make check. Alternative instructions in mbedtls-2.1.1/README.md
* Edit aws-iot-device-sdk-embedded-C-master/aws_iot_config.h
NOTE: aws_iot_config.h needs to be edited with your certification details
	   * AWS_IOT_MQTT_HOST: This is found from the AWS IoT console, Registry, Things, <your_thing>,
				Click interact on the left panel, and under HTTPS is the Rest API Endpoint
	   * AWS_IOT_MQTT_PORT: You can keep default
	   * AWS_IOT_MQTT_CLIENT_ID: Click on "Details" under your thing on AWS IoT console,
				     under Thing ARN, your client id is the name after ":thing/"
	   * AWS_IOT_MY_THING_NAME:  Same as AWS_IOT_MQTT_CLIENT_ID
	   * AWS_IOT_ROOT_CA_FILENAME:  Unless you changed the name then it most likely is root-CA.crt
	   * AWS_IOT_CERTIFICATE_FILENAME:  Your downloaded file may look like <random_characters>.cert.pem
					    you can edit the file so that it reads <THING>.cert so that it is uniqely identified
	   * AWS_IOT_PRIVATE_KEY_FILENAME:  Same as the CERTIFICATE but enter the private key.  If you dont change the name then 
					    enter it similiarly to how you downloaded it.

/ USER CONFIG EXAMPLE:  Obtain certification after registering "thing" on AWS IoT.
// =================================================
#define AWS_IOT_MQTT_HOST              "xxxxxxxxxxxxx.iot.us-xxxx-x.amazonaws.com" ///< Customer specific MQTT HOST. The same will be used for Thing Shadow
#define AWS_IOT_MQTT_PORT              8883 ///< default port for MQTT/S
#define AWS_IOT_MQTT_CLIENT_ID         "THING" ///< MQTT client ID should be unique for every device
#define AWS_IOT_MY_THING_NAME              "THING" ///< Thing Name of the Shadow this device is associated with
#define AWS_IOT_ROOT_CA_FILENAME       "root-CA.crt" ///< Root CA file name
#define AWS_IOT_CERTIFICATE_FILENAME   "THING.cert.pem" ///< device signed certificate file name
#define AWS_IOT_PRIVATE_KEY_FILENAME   "THING.private.key" ///< Device private key filename
// =================================================

* Explore /aws-iot-device-sdk-embedded-C-master/ and view README.md for inisght

####nRF24/RF24Mesh
* Change to Base Directory
* Download RF24Mesh-master.zip
   * wget https://github.com/nRF24/RF24Mesh/archive/master.zip
   * unzip master.zip
* Explore RF24Mesh-master contents and https://tmrh20.github.io/RF24Mesh/ for insight

####Understanding AWS IoT, obtaining x.509 certificates and AWS credentials

 * Read:    [AWS IoT developer guide](http://docs.aws.amazon.com/iot/latest/developerguide/iot-security-identity.html)
 * Sign Up: [AWS IoT Services](https://aws.amazon.com/iot/)
 * Connect IoT Device:
    * From Main AWS Console: Find "Build a solution" and "Connect an IoT Device"
	  or From AWS IoT Dashboard: Click on Registry (Left Column) and "Create" (Top Right)
    * Certification:  
       * Click "Security" under your newly created Thing and "Create certificate"
       * Download: thing certificate, public key, private key and root CA for AWS IoT from Symantec
         Note: All four of these will go under "aws-iot-device-sdk-embedded-C/certs" on Raspberry Pi
       * Click "Activate certificate"
       * Click "Attach Policy"
       * Click "Create new policy": 
          * Policy name can be your THING name
	  * Action: "iot:*, dynamodb:*"
          * Resource: "*"
		
###Setting up AWS IoT Rules and dynamoDB
  * From AWS IoT Console (https://aws.amazon.com/iot/):  Click on "Rules" in left column and "Create" on upper right
  * Fill in Create a rule:
     * Name: Some description e.g. (<insert_thing_name>TempData)
     * Using SQL Version: 2016-03-23
     * Rule query statement: 
        * Attribute: *
	* Topic Filter: "Sensor/temp/#"
	NOTE: '#', '+', and '*' are wildcards for topics
	* Condition: Leave blank, later on you can change this e.g. (temperature > 75)
     * Add Action
	* Insert a message into a DynamoDB table
	   * Click "Create a new resource"
	   * Click "create table"
	   * Table Name: e.g. (sensorTemp or nodeTemp, etc.)
	   * Partition Key: "nodeID" as a String, click check box for Add sort key and enter "timestamp" as a String
	   * Use default settings
	   * Create
        * Choose your newly created Table under "Table name"
	* In *Hash key value, fill in "${topic(3)}" without the quotes
	NOTE: This means when you publish to the topic chosen for the rule, the third entry of the topic will be selected to partition
	      the dynamoDB under nodeID. e.g(<nodeID> will be the third topic for Sensor/temp/<nodeID>). Since the nodes will send their
	      unique id in their payload, the topic can dynamically change based on various nodeIDs and have that payload sorted into dynamoDB.
	* In "Range key value", fill in "${timestamp()}
	NOTE: This will add a timestamp to the published payload message so that dynamoDB fills up based on timestamp instead of overwriting the same
	      field everytime a new payload message is published.
	* Create a new role under *IAM role name
	NOTE: IAM is how AWS grants users and groups permissions for various services.
    NOTE: An alternative method of setting up Rules and AWS IoT in general is using the AWS CLI and json files.
	  [AWS CLI](https://aws.amazon.com/cli/)
	  [JSON for Parameters](https://docs.aws.amazon.com/cli/latest/userguide/cli-using-param.html#cli-using-param-json)

