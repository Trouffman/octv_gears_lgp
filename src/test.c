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

struct commandframe {
	size_t expectanswer;
	size_t endpoint;
	size_t size;
	unsigned char command[32768];
};

static const char *captureconfigfile = NULL;

int writecommand(libusb_device_handle *camerahandle, unsigned char* commandbuffer, size_t size) {
	static int transferred = 0;
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
		for(size_t i = 0; i < transferred; i++)
			fprintf(stderr,"%.2x ", buffer[i]);
	
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
			// If there is a trailing space at the end the size will be incremented too. This send the data length + 1 - NB : should be corrected.
		}
		commandi++;
	}
		
	free(line);
	fclose(f);

	return 0;
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
	// 6. Comm
	// 7. Cleanup


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

	// 6. Comm
	unsigned char id_00100[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x0b  };
	size_t id_00100_s = 10;	
	unsigned char id_00101[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x03  };
	size_t id_00101_s = 10;	
	unsigned char id_00102[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x05  };
	size_t id_00102_s = 10;

	// Boot (LED show)
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

	
	// Try to start streaming	
	
	for(size_t i = 0; i < capturepacketcount; i++) {
		// Check if we want to send a request or just listen to one.
		if(capturepackets[i].size > 0) {
			fprintf(stderr,"Sending : step %zu on endpoint %zu with size %zu :", i, capturepackets[i].endpoint, capturepackets[i].size);
			for(size_t j = 0; j < capturepackets[i].size; j++)
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

		fprintf(stderr,"Capture stream sent, will try to capture stuff on other endpoint now...\n");
		outputfile = fopen("capture.h264", "wb");
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

	// 7. Cleanup
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
