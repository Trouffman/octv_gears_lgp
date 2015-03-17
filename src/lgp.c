#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

#define check(A, M, ...) \
		do { \
			if(!(A)) { \
				fprintf(stderr, "Check failed at %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
				goto error; \
			} \
		} while(0)

#define CAMERA_VENDOR				0x07ca
#define CAMERA_PRODUCT				0x0875
#define CAMERA_CONFIGURATION		1
#define CAMERA_INTERACE				0

#define CAMERA_ENDPOINT_ADDRESS_VIDEO_CAPTURE		0x81
#define CAMERA_ENDPOINT_ADDRESS_VIDEO_CONTROL		0x02
#define CAMERA_ENDPOINT_ADDRESS_CONTROL				0x04
#define CAMERA_ENDPOINT_ADDRESS_STATUS_RESPONSE		0x83

#define TIMEOUT		1000

#define DEBUG		1

struct commandframe {
	size_t expectanswer;
	size_t endpoint;
	size_t size;
	unsigned char command[32768];
};

static const char *captureconfigfile = NULL;

int writecommand(libusb_device_handle *camerahandle, unsigned char* commandbuffer, size_t size) {
	static int transferred = 0;
	
	// Debug only
	if (DEBUG) {
		fprintf(stderr,"Sending : ");
		for(size_t j = 0; j < sizeof(commandbuffer) ; j++)
			fprintf(stderr,"%.2x ", commandbuffer[j]);
		fprintf(stderr,"\n");
	}
	// End Debug only

	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_CONTROL, commandbuffer, size, &transferred, TIMEOUT);
	if(err != 0) {
		fprintf(stderr, "Error while sending command: '%s' - '%s', data sent: %i, data transferred: %i, on endpoint 0x04, crashing!\n", libusb_error_name(err), libusb_strerror(err), 512, transferred);
		exit(-5);
	} else {
		return 0;
	}
}

int writevideocommand(libusb_device_handle *camerahandle, unsigned char* commandbuffer, size_t size) {
	static int transferred = 0;
	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_VIDEO_CONTROL, commandbuffer, size, &transferred, TIMEOUT);
	if(err != 0) {
		fprintf(stderr, "Error while sending command: '%s' - '%s', data sent: %i, data transferred: %i, on endpoint 0x02, crashing!\n", libusb_error_name(err), libusb_strerror(err), 512, transferred);
		exit(-5);
	} else {
		return 0;
	}
}

int readstatus(libusb_device_handle *camerahandle) {
	static unsigned char buffer[512];
	memset(buffer, 0, 512);
	static int transferred = 0;

	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_STATUS_RESPONSE, buffer, 512, &transferred, TIMEOUT);

	if(err != 0 && err != LIBUSB_ERROR_TIMEOUT) {
		fprintf(stderr, "Error while reading command: '%s' - '%s' , data received: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err),transferred);
		exit(-5);
	}

	if(transferred == 0) {
		fprintf(stderr,"Status NOT received (EP. 83); TIMEOUT!\n");
	} else {
		fprintf(stderr,"Received Status Control : bytes %i!, value :", transferred);
		for(size_t i = 0; i < transferred; i++)
			fprintf(stderr,"%.2x ", buffer[i]);
		fprintf(stderr,"\n");
	}
	return 0;
}

int readstatus_data(libusb_device_handle *camerahandle, char *response_buffer) {
	static unsigned char buffer[512];
	memset(buffer, 0, 512);
	static int transferred = 0;

	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_STATUS_RESPONSE, buffer, 512, &transferred, TIMEOUT);

	if(err != 0 && err != LIBUSB_ERROR_TIMEOUT) {
		fprintf(stderr, "Error while reading command: '%s' - '%s' , data received: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err),transferred);
		exit(-5);
	}

	if(transferred == 0) {
		fprintf(stderr,"Status NOT received (EP. 83); TIMEOUT!\n");
	} else {
		fprintf(stderr,"Received Status Control : bytes %i!, value :", transferred);
		for(size_t i = 0; i < transferred; i++) {
			fprintf(stderr,"%.2x ", buffer[i]);
		}
		fprintf(stderr,"\n");
		memcpy(response_buffer,buffer,transferred);
		fprintf(stderr,"Data copied to returned buffer.\n");
	}
	return 0;
}

int readvideostatus(libusb_device_handle *camerahandle) {
	static unsigned char buffer[32768];
	memset(buffer, 0, 32768);
	static int transferred = 0;

	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_VIDEO_CAPTURE, buffer, 32768, &transferred, TIMEOUT);

	if(err != 0 && err != LIBUSB_ERROR_TIMEOUT) {
		fprintf(stderr, "Error while reading command: '%s' - '%s' , data received: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err),transferred);
		exit(-5);
	}

	if(transferred == 0) {
		fprintf(stderr,"Status NOT received (EP. 81); TIMEOUT!\n");
	} else {
		fprintf(stderr,"Received Status Video Control : bytes %i!, value :", transferred);
		//for(size_t i = 0; i < transferred; i++)
		//	fprintf(stderr,"%.2x ", buffer[i]);

	
		fprintf(stderr,"\n");
	}
	return 0;
}

int readvideostream(libusb_device_handle *camerahandle, FILE *outputfile) {
	static unsigned char buffer[32768];
	memset(buffer, 0, 32768);
	static int transferred = 0;

	int err = 0;
lelabel: err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_VIDEO_CAPTURE, buffer, 32768, &transferred, TIMEOUT);

	if(err != 0) {
		if(err == LIBUSB_ERROR_TIMEOUT) {
			fprintf(stderr, "Timeout!\n");
			goto lelabel;
		}
		fprintf(stderr,"Error while reading capture stream: '%s' - '%s' , data received: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err),transferred);
		return -1;
	}

	fwrite(buffer, transferred, transferred, outputfile);
	return 0;
}

int readcapturesequence(struct commandframe **capturepackets,size_t *capturepacketcount) {
	fprintf(stderr, "Reading capture sequence...\n");
	FILE *f = fopen(captureconfigfile, "r");
	if(f == NULL) {
		fprintf(stderr, "Failed to open file: capture_sequence!\n");
		return -1;
	}

	size_t linebuffersize = 2048;
	char *line = (char*)malloc(linebuffersize);
	size_t linesize;
	size_t linecount = 0;
	while((linesize = getline(&line, &linebuffersize, f)) != -1)  {
		// Just count the lines to begin with
		linecount++;
	}
	*capturepacketcount = linecount;

	// Parse
	fseek(f, 0, SEEK_SET);
	*capturepackets = (struct commandframe*) malloc(sizeof(struct commandframe) * linecount);
	struct commandframe *cp = *capturepackets;
	memset(cp, 0, sizeof(struct commandframe) * linecount);
	size_t commandi = 0;
	while((linesize = getline(&line, &linebuffersize, f)) != -1)  {
		char *tok = line;
		tok = strtok(tok, " ");
		cp[commandi].expectanswer = atoi(tok);
		
		// Added to support destination Endpoint for the sequence capture file
		tok = strtok(NULL, " ");
		cp[commandi].endpoint = atoi(tok);
		// fprintf(stderr, "Endpoint : %zu ", cp[commandi].endpoint);
		
		// Read bytes
		while((tok = strtok(NULL, " \n")) != NULL) {
			// Skip leading zero because it seems strtol gets lost
			//if(*tok == '0')
			//	tok++;
			int byte = strtol(tok, NULL, 16);
			//fprintf(stderr, "Byte: %.2x\n", byte);
			cp[commandi].command[cp[commandi].size] = byte;
			cp[commandi].size++;

		}
		commandi++;
	}
		
	free(line);
	fclose(f);

	return 0;
}

int load_firmware(const char *file) {
	int transfer;

	FILE *bin;
	bin = fopen(file, "rb");

	/* get filesize */
	fseek(bin, 0L, SEEK_END);
	long filesize = ftell(bin);
	rewind(bin);

	/* read firmware from file to buffer and bulk transfer to device */
	for (int i = 0; i <= filesize; i += DATA_BUF) {
		unsigned char data[DATA_BUF] = {0};
		int bytes_remain = filesize - i;

		if ((bytes_remain) > DATA_BUF) {
			bytes_remain = DATA_BUF;
		}

		fread(data, bytes_remain, 1, bin);

		libusb_bulk_transfer(camerahandle, 0x02, data, bytes_remain, &transfer, 0);
	}

	fclose(bin);
}


int main(int argc, char **argv) {
	FILE *outputfile = NULL;
	libusb_context *usbcontext = NULL;
	libusb_device **devicelist;
	libusb_device_handle *camerahandle = NULL;
	struct commandframe *capturepackets = NULL;
	size_t capturepacketcount = 0;
	int err = 0;

	check(argc >= 2, "You need to specify capture config file.");
	captureconfigfile = argv[1];

	err = readcapturesequence(&capturepackets, &capturepacketcount);
	check(err == 0, "Error reading config packets!");
	fprintf(stderr,"Config done, %zu packets read; sending the capture command packets...\n", capturepacketcount);

	// Process:
	// 1. Grab USB context
	// 2. Query system devices
	// 3. Figure out which one is the camera
	// 4. Configure the camera
	// 5. Claim interfaces
	// 6. Initialization
	// 7. Capture
	// 8. Cleanup



	// 1. Grab USB context
	check(libusb_init(&usbcontext) == 0, "We DONT have the context");
	fprintf(stderr,"We got the context\n");

	libusb_set_debug(usbcontext, LIBUSB_LOG_LEVEL_WARNING);
	
	// 2. Query system devices
	size_t devicecount = libusb_get_device_list(usbcontext, &devicelist);
	check(devicecount != 0, "Error when counting devices!");

	// 3. Figure out which one is the camera
	for(size_t i = 0; i < devicecount; i++) {
		libusb_device *dev = devicelist[i];	
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(dev, &desc);

		if(desc.idVendor == CAMERA_VENDOR && desc.idProduct == CAMERA_PRODUCT) {
			fprintf(stderr,"Found camera!\n");
			if(libusb_open(dev, &camerahandle) != 0) {
				fprintf(stderr,"Error getting the handle!\n");
				camerahandle = NULL;
			}
		}
	}

	check(camerahandle != NULL, "Couldn't obtain camera handle.");

	// 4. Configure the camera
	check(libusb_set_configuration(camerahandle, CAMERA_CONFIGURATION) == 0,"Failed to set configuration!");

	// 5. Claim interfaces
	check(libusb_claim_interface(camerahandle, CAMERA_INTERACE) == 0,"Failed to claim interface!");

	// 6. Initialization sequence

	unsigned char id_00100[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x0b  };
	size_t id_00100_s = 10;	
	unsigned char id_00101[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x03  };
	size_t id_00101_s = 10;	
	unsigned char id_00102[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x05  };
	size_t id_00102_s = 10;

	// Frames Data with unified IDs

	//unsigned char id_40009[] = { 0x01, 0x01, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00 };
	//size_t id_40009_s = 12;


	// Answer data 
	//unsigned char id_30010[] = { 0x07, 0x00, 0x00, 0x00 };
	//size_t id_30010_s = 4;


	// Boot
	/*
	fprintf(stderr,"Init procedure...\n");
	writecommand(camerahandle, id_00100, id_00100_s);
	readstatus(camerahandle);
	sleep(1);

	writecommand(camerahandle, id_00101, id_00101_s);
	readstatus(camerahandle);
	sleep(1);

	writecommand(camerahandle, id_00102, id_00102_s);
	readstatus(camerahandle);
	sleep(1);
	fprintf(stderr,"Done\n");
	*/
	
	// Send the initialization sequence	

	for(size_t i = 0; i < capturepacketcount; i++) {
		// Check if we want to send a request or just listen to one.
		if(capturepackets[i].size > 0) {
			fprintf(stderr,"Sending : step %zu on endpoint %zu with size %zu :", i, capturepackets[i].endpoint, capturepackets[i].size);
			for(size_t j = 0; j < capturepackets[i].size && j < 64 ; j++)
				fprintf(stderr,"%.2x ", capturepackets[i].command[j]);
			fprintf(stderr,"\n");
			
			if(capturepackets[i].endpoint == 2) {
				writevideocommand(camerahandle, capturepackets[i].command, capturepackets[i].size);
				if(capturepackets[i].expectanswer)
					readvideostatus(camerahandle);
			} else {
				writecommand(camerahandle, capturepackets[i].command, capturepackets[i].size);
				if(capturepackets[i].expectanswer)
					readstatus(camerahandle);
			}
		} else {
			if(capturepackets[i].endpoint == 81 && capturepackets[i].expectanswer)
				readvideostatus(camerahandle);
			if(capturepackets[i].endpoint == 83 && capturepackets[i].expectanswer)
				readstatus(camerahandle);
		}
	}

	// 7. Capturing video
	// Adding logic to get video feed
	fprintf(stderr,"Capture procedure...\n");
	
	// Handshake : Is video ready?
	unsigned char id_40001[] = { 0x01, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00 };
	size_t id_40001_s = 8;
	
	// Frame :  Reset video ready handshake
	unsigned char resetvideo[] = { 0x01, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00 };
	size_t resetvideo_s = 12;
	
	// Frame : Request key video data
	unsigned char id_40007[] = { 0x01, 0x00, 0x07, 0x00, 0xB0, 0x06, 0x00, 0x00 };
	size_t id_40007_s = 8;
	
	unsigned char id_40025[] = { 0x01, 0x01, 0x01, 0x00, 0xC8, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	size_t id_40025_s = 12;
	
	unsigned char response_buffer[512];
	memset(response_buffer, 0, 512);
	unsigned char videoready_received = 0;
	unsigned char videoready_response[] = { 0x07, 0x00, 0x01, 0x00 };
	
	unsigned char videokeyframe_data[3];
	memset(videokeyframe_data, 0, 3);
	unsigned char videokeyframe_data2[2];
	memset(videokeyframe_data2, 0, 2);
	
	// Loop the handshake and wait for the video signal to be ready
	//int do_next = 0;
	while ( videoready_received == 0 ) {
		fprintf(stderr,"Sending data video handshake...\n");
		writecommand(camerahandle, id_40001, id_40001_s);
		readstatus_data(camerahandle, response_buffer);
		
		// Compare the received signal with the one expected
		int is_identical = 1;
		
		for(size_t i = 0; i < sizeof(videoready_response); i++) {
			// Got the signal the video is ready to be captured, we need to have it twice.
			if(response_buffer[i] != videoready_response[i]) is_identical = 0; 
		}
		if (is_identical) {
			videoready_received = 1;
			
			// Reset the video is ready signal
			fprintf(stderr,"Reset videoready signal\n");
			writecommand(camerahandle, resetvideo, resetvideo_s);
			readstatus(camerahandle); // no status expected
		}
		// Avoid flooding the device
		sleep(1);
	}
	
	// reset value of the buffer cache for comparison
	memset(response_buffer, 0, 512);
	int is_firsttime = 1;
	
	unsigned char videosync_info[] = { 0x09, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 };
	
	// debug only limit number of execution
	int limit = 5;
	
	while(limit >=0) {
		// videoready received twice
		fprintf(stderr,"Video ready received (and confirmed)!\n");
	
		
		// Ask for Frame key data
		fprintf(stderr,"Requesting Video Key Frame data\n");
		writecommand(camerahandle, id_40007, id_40007_s);
		readstatus_data(camerahandle, response_buffer);
		
		// Extract part of the data we want :
		// part one
		fprintf(stderr,"Video Key Frame Data important info :\n");
		memcpy(videokeyframe_data, response_buffer+8, 3);
		fprintf(stderr,"%.2x ", videokeyframe_data[0]);
		fprintf(stderr,"%.2x ", videokeyframe_data[1]);
		fprintf(stderr,"%.2x ", videokeyframe_data[2]);
		fprintf(stderr,"\n");

		// part two
		memcpy(videokeyframe_data2, response_buffer+16, 2);
		fprintf(stderr,"%.2x ", videokeyframe_data2[0]);
		fprintf(stderr,"%.2x ", videokeyframe_data2[1]);
		fprintf(stderr,"\n");
		
		// Debug only
		fprintf(stderr,"Blinking... Not dead!\n");
		writecommand(camerahandle, id_00100, id_00100_s);
		readstatus(camerahandle);
		sleep(1);
		// End Debug
		
		
		// Send frame to prepare for data sync.
		fprintf(stderr,"Static frame\n");
		writecommand(camerahandle, id_40025, id_40025_s);
		readstatus(camerahandle);
		
		
		// Sending the sync command
		if (is_firsttime) {
			fprintf(stderr," First time to send Video Key Frame info \n");
			// Create the data to be sent with video key frame info.
			videosync_info[8] = videokeyframe_data[0];
			videosync_info[9] = videokeyframe_data[1];
			videosync_info[10] = videokeyframe_data[2];
			videosync_info[12] = 0x00;
			videosync_info[13] = 0x80;

			
			// First time send semi-fixed data
			fprintf(stderr,"Send Video Key Frame info (first time) \n");
			writecommand(camerahandle, videosync_info, 16);
			readstatus(camerahandle);
			
			// Debug only
			fprintf(stderr,"Blinking... Not dead!\n");
			writecommand(camerahandle, id_00101, id_00101_s);
			readstatus(camerahandle);
			sleep(1);
			// End Debug
			
			is_firsttime = 0;
			continue;
		}
		
		// Send the sync command with all the video key frame info
			videosync_info[8] = videokeyframe_data[0];
			videosync_info[9] = videokeyframe_data[1];
			videosync_info[10] = videokeyframe_data[2];
			videosync_info[12] = videokeyframe_data2[0];
			videosync_info[13] = videokeyframe_data2[1];
			
			fprintf(stderr,"Send Video Key Frame info \n");
			writecommand(camerahandle, videosync_info, 16);
			readstatus(camerahandle);
		
		// listen to video stream input EP.
		
		// do this over and over.
		limit--;
	}
	
	
	
	// Capture video in file
	fprintf(stderr,"Capture stream sent, will try to capture stuff on other endpoint now...\n");
	outputfile = fopen("capture.h264", "w+b");
	if(outputfile != NULL) { 
		int running = 1;
		while(running) {
			err = readvideostream(camerahandle, outputfile);
			if(err != 0) {
				running = 0;
				fprintf(stderr,"ERROR WITH STREAM CAPTURE, ABORT!\n");
			}
		}
	} else {
		fprintf(stderr,"Failed to open capture file, obviously - aborting!\n");
	}	

	// 8. Cleanup
error:
	if(outputfile != NULL) {
		fprintf(stderr,"Closing capture file...\n");
		fclose(outputfile);
	}

	free(capturepackets);

	fprintf(stderr,"Closing handles...\n");
	if(camerahandle != NULL) {
		libusb_release_interface(camerahandle, CAMERA_INTERACE);
		libusb_close(camerahandle);
	}
	if(devicelist != NULL)
		libusb_free_device_list(devicelist, 1); // 1 = unref devices

	fprintf(stderr,"Bye\n");
	libusb_exit(usbcontext);
	return 0;
}
