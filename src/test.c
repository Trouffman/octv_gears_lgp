#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

#define CAMERA_VENDOR				0x07ca
#define CAMERA_PRODUCT				0x0875	
#define CAMERA_CONFIGURATION		1
#define CAMERA_INTERACE				0

#define CAMERA_ENDPOINT_ADDRESS_VIDEO_CAPTURE		0x81
#define CAMERA_ENDPOINT_ADDRESS_CONTROL				0x04
#define CAMERA_ENDPOINT_ADDRESS_STATUS_RESPONSE		0x83

#define TIMEOUT		1000

struct commandframe {
	size_t expectanswer;
	size_t size;
	unsigned char command[64];
};

int writecommand(libusb_device_handle *camerahandle, unsigned char* commandbuffer, size_t size) {
	static int transferred = 0;
	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_CONTROL, commandbuffer, size, &transferred, TIMEOUT);
	if(err != 0) {
		fprintf(stderr, "Error while sending command: '%s' - '%s',  data sent: %i, data transferred: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err), 512, transferred);
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
		fprintf(stderr,"Status NOT received; TIMEOUT!\n");
	} else {
		fprintf(stderr,"Status received, bytes %i!, value: ", transferred);
		for(size_t i = 0; i < transferred; i++)
			fprintf(stderr,"%.2x ", buffer[i]);
	
		fprintf(stderr,"\n");
	}
	return 0;
}

int readvideostream(libusb_device_handle *camerahandle, FILE *outputfile) {
	static unsigned char buffer[512];
	memset(buffer, 0, 512);
	static int transferred = 0;

	int err = 0;
lelabel: err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_VIDEO_CAPTURE, buffer, 512, &transferred, TIMEOUT);

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
	FILE *f = fopen("capture_sequence", "r");
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
		char *tok = NULL;
		tok = strtok(line, " ");
		cp[commandi].expectanswer = atoi(tok);

#pragma warning "CRASHES HERE IN STRTOL INTERNALLY!"
		// Read bytes
		while((tok = strtok(line, " ")) != NULL) {
			// Skip leading zero because it seems strtol gets lost
			if(*tok == '0')
				tok++;
			int byte = strtol(tok, NULL, 16);
			cp[commandi].command[cp[commandi].size] = byte;
			cp[commandi].size++;
		}
		commandi++;
	}
		
	free(line);
	fclose(f);

	return 0;
}

int main(int argc, char **argv) {
	// Process:
	// 1. Grab USB context
	// 2. Query system devices
	// 3. Figure out which one is the camera
	// 4. Configure the camera
	// 5. Claim interfaces
	// 6. Comm
	// 7. Cleanup


	// 1. Grab USB context
	libusb_context *usbcontext = NULL;
	if(libusb_init(&usbcontext) == 0)
		fprintf(stderr,"We got the context\n");
	else {
		fprintf(stderr,"We DONT have the context\n");
		exit(1);
	}

	libusb_set_debug(usbcontext, LIBUSB_LOG_LEVEL_WARNING);

	
	// 2. Query system devices
	libusb_device **devicelist;
	size_t devicecount = libusb_get_device_list(usbcontext, &devicelist);
	if(devicecount < 0) {
		fprintf(stderr,"Error when counting devices!\n");
		exit(-1);
	}

	// 3. Figure out which one is the camera
	libusb_device_handle *camerahandle = NULL;
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


	if(camerahandle == NULL) {
		fprintf(stderr,"Couldn't obtain camera handle.\n");
		exit(-2);
	}


	// 4. Configure the camera
	if(libusb_set_configuration(camerahandle, CAMERA_CONFIGURATION) != 0) {
		fprintf(stderr,"Failed to set configuration!\n");
		exit(-3);
	}


	// 5. Claim interfaces
	if(libusb_claim_interface(camerahandle, CAMERA_INTERACE) != 0) {
		fprintf(stderr,"Failed to claim interface!\n");
		exit(-4);
	}

	// 6. Comm

	unsigned char id_00100[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x0b  };
	size_t id_00100_s = 10;	
	unsigned char id_00101[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x03  };
	size_t id_00101_s = 10;	
	unsigned char id_00102[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x05  };
	size_t id_00102_s = 10;

	// Boot
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
	struct commandframe *capturepackets = NULL;
	size_t capturepacketcount = 0;
	int err = readcapturesequence(&capturepackets, &capturepacketcount);
	if(err != 0) {
		fprintf(stderr,"Error reading config packets!\n");
		exit(-1);
	}

	fprintf(stderr,"Config done, %zu packets read; sending the capture command packets...\n", capturepacketcount);
	

	for(size_t i = 0; i < capturepacketcount; i++) {
		// Please add a debug to have the output of the command sent please. 
		// Note Trouff : i dunno how to print the data sent for debugging purpose only.
		fprintf(stderr,"Sending : step %zu with size %zu & data : ", i, capturepackets[i].size);
		for(size_t j = 0; j < capturepackets[i].size; j++)
			fprintf(stderr,"%.2x ", capturepackets[i].command[j]);
		fprintf(stderr,"\n");

		writecommand(camerahandle, capturepackets[i].command, capturepackets[i].size);
		if(capturepackets[i].expectanswer)
			readstatus(camerahandle);
	//	sleep(5);
	}

	fprintf(stderr,"Capture stream sent, will try to capture stuff on other endpoint now...\n");
	FILE *outputfile = fopen("capture.h264", "wb");
	if(outputfile != NULL) { 
		int running = 1;
		while(running) {
			int err = readvideostream(camerahandle, outputfile);
			if(err != 0) {
				running = 0;
				fprintf(stderr,"ERROR WITH STREAM CAPTURE, ABORT!\n");
			}
		}
	} else {
		fprintf(stderr,"Failed to open capture file, obviously - aborting!\n");
	}	

	// 7. Cleanup
	if(outputfile != NULL) {
		fprintf(stderr,"Closing capture file...\n");
		fclose(outputfile);
	}

	free(capturepackets);

	fprintf(stderr,"Closing handles...\n");
	libusb_release_interface(camerahandle, CAMERA_INTERACE);
	libusb_close(camerahandle);
	libusb_free_device_list(devicelist, 1); // 1 = unref devices
	libusb_exit(usbcontext);
	fprintf(stderr,"Bye\n");
	return 0;
}
